// Editor-only console bindings and the open level's command services.
// Undo replays silent console scripts; interactive commands may return query output.
// Command syntax, save behavior, and integration: docs/editor-console.md.
#ifndef BF2EDITOR_FRAMEWORK_EDITORCONSOLE_H
#define BF2EDITOR_FRAMEWORK_EDITORCONSOLE_H

#include "Framework/EditorConfig.h"

#include "Math/Mat4.h"
#include "Math/Vec3.h"

#include <string>
#include <vector>

namespace bf2editor
{

// What the editor's console commands act on; the edit session of the open level implements it.
// Every method returns false and fills `error` when it did not do what was asked.
class IEditorCommands
{
public:
    virtual ~IEditorCommands() {}

    virtual bool activateObject(unsigned id, std::string& error) = 0;
    virtual bool selectObject(unsigned id, bool add, std::string& error) = 0;   // 0 without add clears
    // An object reference as scripts write it: a number is the object id; `template` or
    // `template:n` is the n-th (from 0) live root object of that template in id order - a reference
    // that survives engine changes which renumber objects.
    virtual bool resolveObject(const std::string& reference, unsigned& id, std::string& error) = 0;
    // Picks the object under window pixel (x, y) and selects it; `result` says what was hit.
    virtual bool pickAt(float x, float y, std::string& result) = 0;
    // The same, adding to the selection (Shift+click).
    virtual bool pickAdd(float x, float y, std::string& result) = 0;
    // Every root object of a template ("select all of type").
    virtual bool selectTemplate(const std::string& name, std::string& result) = 0;
    // The selected objects' templates, ids, positions, rotations and layers, as text (a query).
    virtual bool selectionInfo(std::string& result) = 0;
    virtual bool translateSelection(const dice::hfe::Vec3& delta, std::string& error) = 0;
    virtual bool rotateSelection(const dice::hfe::Vec3& yawPitchRollDelta, std::string& error) = 0;
    virtual bool setSelectionPosition(const dice::hfe::Vec3& position, std::string& error) = 0;
    virtual bool setSelectionRotation(const dice::hfe::Vec3& yawPitchRoll, std::string& error) = 0;
    virtual bool undo(std::string& error) = 0;
    virtual bool redo(std::string& error) = 0;
    virtual void clearHistory(const std::string& reason) = 0;
    virtual bool frameSelection(std::string& error) = 0;
    virtual bool setGizmoMode(int mode, std::string& error) = 0;
    virtual bool setGizmoSpace(int space, std::string& error) = 0;
    // Layers: hiding and locking are the editor's view; setSelectionLayer is an undoable edit.
    virtual bool setLayerHidden(int layer, bool hidden, std::string& error) = 0;
    virtual bool setLayerLocked(int layer, bool locked, std::string& error) = 0;
    virtual bool selectLayer(int layer, std::string& error) = 0;
    virtual bool setSelectionLayer(int layer, std::string& error) = 0;
    // The level's objects file (WP6): what a save would write, and the save itself. `result` says
    // what happened; false when something could not be saved.
    virtual bool levelStatus(std::string& result) = 0;
    virtual bool saveLevel(std::string& result) = 0;
    // The saved level packed for the game into <mod folder>/Levels/<name>/ (Framework/LevelPacker); refused
    // while there are unsaved changes and into the editor's own mod folder.
    virtual bool packLevel(const std::string& modFolder, std::string& result) = 0;
    // The saved level copied under a new name (Framework/LevelCopier); with `open`, the editor starts
    // again on the copy. Refused while there are unsaved changes.
    virtual bool saveLevelAs(const std::string& name, bool open, std::string& result) = 0;
    // A new level beside the open one (Framework/NewLevel): `terrain` is "size/scale/height/lowResSize" or "default"
    // (the retail dialog's 1024/2/25/256); with `open`, the editor starts again on it.
    virtual bool createLevel(const std::string& name, const std::string& terrain, bool open, std::string& result) = 0;
    // Objects made and unmade (WP6): the user's edits, and the replays the history's scripts run.
    virtual bool createObject(const std::string& templateName, const dice::hfe::Vec3& position,
                              const dice::hfe::Vec3& yawPitchRoll, int layer, std::string& result) = 0;
    virtual bool deleteSelection(std::string& result) = 0;
    virtual bool recreateObject(unsigned originalId, const std::string& templateName,
                                const dice::hfe::Mat4& transform, int layer, std::string& error) = 0;
    virtual bool removeObject(unsigned id, std::string& error) = 0;
    // Placement (WP6 part 2): a new object on the surface under window pixel (x, y) (a drop from the
    // Resources panel); copies of the selection where it stands, which become the selection; the
    // selection set down on the terrain under each object. `result` says what was done.
    virtual bool placeObjectAt(const std::string& templateName, float x, float y, std::string& result) = 0;
    virtual bool duplicateSelection(std::string& result) = 0;
    virtual bool snapSelectionToGround(std::string& result) = 0;
    // Phase A: box selection (mode 0 replace, 1 add, 2 toggle); replacing the selected
    // objects' template; a transformation copied from the primary and pasted onto the selection; X and
    // Z snapped to a grid; the object clipboard (pasted at the view centre unless `atGiven`).
    virtual bool selectInBox(float x0, float y0, float x1, float y1, int mode, std::string& result) = 0;
    virtual bool replaceSelection(const std::string& templateName, std::string& result) = 0;
    virtual bool copyTransform(std::string& result) = 0;
    virtual bool pasteTransform(std::string& result) = 0;
    virtual bool snapSelectionToGrid(float step, std::string& result) = 0;
    virtual bool copySelection(std::string& result) = 0;
    virtual bool cutSelection(std::string& result) = 0;
    virtual bool pasteClipboard(const dice::hfe::Vec3& at, bool atGiven, std::string& result) = 0;
    // The camera (phase A): its mode (0 free, 1 top-down, 2 orbit, 3 follow the terrain), position and
    // speed.
    virtual bool setCameraMode(int mode, std::string& result) = 0;
    virtual bool setCameraPosition(const dice::hfe::Vec3& position, std::string& result) = 0;
    virtual bool resetCameraPosition(std::string& result) = 0;
    virtual bool setCameraSpeed(float metresPerSecond, std::string& result) = 0;
    virtual bool cameraInfo(std::string& result) = 0;       // a query: position, heading, speed, mode
    virtual bool cameraBookmark(const std::string& action, int index, std::string& result) = 0;
    virtual bool lightPreset(const std::string& action, const std::string& argument, std::string& result) = 0;
    virtual bool materialCommand(const std::string& action, const std::vector<std::string>& args, std::string& result) = 0;
    virtual bool hostTool(const std::string& action, const std::string& argument, std::string& result) = 0;
    virtual bool applyMaterialEdit(int id, const std::string& state, std::string& error) = 0;
    // The retail layer model's layers (EditorLayer.status | list | add | rename | filters | disable | active | remove).
    virtual bool layersCommand(const std::string& action, const std::vector<std::string>& args, std::string& result) = 0;
    // The animation editor (EditorAnimation.*): the open system's files edited in place, undoable.
    virtual bool animationCommand(const std::string& action, const std::vector<std::string>& args, std::string& result) = 0;
    virtual bool applyAnimationEdit(int id, const std::string& state, std::string& error) = 0;
    // Phase B: a per-object property of every selected object (group, disableChildren, notInAI,
    // isOvergrowth, lightSourceMask), as one undoable edit.
    virtual bool setSelectionProperty(const std::string& name, const std::string& value, std::string& result) = 0;
    // Phase C: markers for the objects that have no geometry (spawners, spawn points, control points,
    // triggers, effect areas), drawn and pickable.
    virtual bool setMarkersShown(bool shown, std::string& error) = 0;
    // Phase C: a line of a template a level file defines (the gameplay layer's spawners, spawn points
    // and control points; AmbientObjects.con's triggers, effect areas and sounds). `set` is the user's
    // undoable edit; `apply` and `remove` are the history's replays; `lines` answers the block.
    virtual bool setTemplateProperty(const std::string& name, const std::string& property, const std::string& value,
                                     std::string& result) = 0;
    virtual bool applyTemplateProperty(const std::string& name, const std::string& property, const std::string& value,
                                       std::string& error) = 0;
    virtual bool removeTemplateLine(const std::string& name, const std::string& key, std::string& error) = 0;
    virtual bool templateInfo(const std::string& name, std::string& result) = 0;
    // Phase C: a new gameplay object - `kind` ObjectSpawner, SpawnPoint or ControlPoint - with a template
    // of its own named `name`, at a point or under the middle of the view. Undoable.
    virtual bool createGameplayObject(const std::string& kind, const std::string& name,
                                      const dice::hfe::Vec3& position, std::string& result) = 0;
    virtual bool createGameplayObjectHere(const std::string& kind, const std::string& name, std::string& result) = 0;
    // An effect area, with its effect (a Bundle or EffectBundle template).
    virtual bool createEffectArea(const std::string& name, const std::string& effect, const dice::hfe::Vec3& position,
                                  std::string& result) = 0;
    virtual bool createEffectAreaHere(const std::string& name, const std::string& effect, std::string& result) = 0;
    // Phase C: a template a level file defines renamed with its objects (one undoable edit); the replay
    // the history runs.
    virtual bool renameLevelTemplate(const std::string& from, const std::string& to, std::string& result) = 0;
    virtual bool applyTemplateRename(const std::string& from, const std::string& to, std::string& error) = 0;
    // Phase C: combat areas (the gameplay layer's CombatArea blocks).
    virtual bool listCombatAreas(std::string& result) = 0;
    virtual bool combatAreaInfo(const std::string& name, std::string& result) = 0;
    virtual bool createCombatArea(const std::string& name, const dice::hfe::Vec3& centre, float size, std::string& result) = 0;
    virtual bool createCombatAreaHere(const std::string& name, float size, std::string& result) = 0;
    virtual bool deleteCombatArea(const std::string& name, std::string& result) = 0;
    virtual bool moveCombatAreaPoint(const std::string& name, int index, float x, float z, std::string& result) = 0;
    virtual bool insertCombatAreaPoint(const std::string& name, int index, float x, float z, std::string& result) = 0;
    virtual bool removeCombatAreaPoint(const std::string& name, int index, std::string& result) = 0;
    virtual bool setCombatAreaProperty(const std::string& name, const std::string& property, int value,
                                       std::string& result) = 0;
    virtual bool setCombatAreaManager(const std::string& property, float value, std::string& result) = 0;
    virtual bool syncCombatArea(const std::string& name, std::string& error) = 0;
    virtual bool syncCombatAreaManager(std::string& error) = 0;
    virtual bool selectCombatArea(const std::string& name, int point, std::string& result) = 0;
    virtual bool setCombatAreasShown(bool shown, std::string& error) = 0;
    // Phase C: the gameplay layers (switching relaunches the editor on the other one).
    virtual bool listLayers(std::string& result) = 0;
    virtual bool switchLayer(const std::string& gameMode, int size, std::string& result) = 0;
    virtual bool duplicateLayer(const std::string& gameMode, int size, std::string& result) = 0;
    virtual bool layersOverview(std::string& result) = 0;
    virtual bool setCompareLayer(const std::string& gameMode, int size, std::string& result) = 0;
    // Phase C: the ambient sounds (AmbientObjects.con's Sound templates; no engine objects).
    virtual bool listSounds(std::string& result) = 0;
    virtual bool soundInfo(const std::string& name, std::string& result) = 0;
    virtual bool moveSound(const std::string& name, const dice::hfe::Vec3& position, std::string& result) = 0;
    virtual bool createSound(const std::string& name, const std::string& soundFile, const dice::hfe::Vec3& position,
                             std::string& result) = 0;
    virtual bool createSoundHere(const std::string& name, const std::string& soundFile, std::string& result) = 0;
    virtual bool deleteSound(const std::string& name, std::string& result) = 0;
    virtual bool applySoundPosition(const std::string& name, const dice::hfe::Vec3& position, std::string& error) = 0;
    virtual bool removeSound(const std::string& name, std::string& error) = 0;
    virtual bool restoreSound(const std::string& name, std::string& error) = 0;
    virtual bool selectSound(const std::string& name, std::string& result) = 0;
    // Phase D: the level settings (Init.con, Sky.con, Water.con).
    virtual bool levelSetting(const std::string& key, std::string& value, std::string& error) const = 0;
    virtual bool setLevelSetting(const std::string& key, const std::string& value, std::string& result) = 0;
    virtual bool applyLevelSetting(const std::string& key, const std::string& value, std::string& error) = 0;
    virtual bool unsetLevelSetting(const std::string& key, std::string& error) = 0;
    virtual bool listLevelSettings(const std::string& group, std::string& result) = 0;
    virtual bool spawnCameraFromView(std::string& result) = 0;
    // Phase E: the terrain (App/TerrainEditing). The brush settings are key/value pairs (mode, size,
    // strength, raise, lower, height); a stroke is `ticks` brush ticks of 1/30 s at (x, z), recorded as
    // one edit; applyChange is what the history's scripts run.
    virtual bool terrainInfo(std::string& result) = 0;
    virtual bool terrainHeight(float x, float z, std::string& result) = 0;
    virtual bool terrainSetBrush(const std::string& key, const std::string& value, std::string& result) = 0;
    virtual bool terrainStroke(float x, float z, int ticks, bool secondary, std::string& result) = 0;
    virtual bool terrainApplyChange(int id, const std::string& state, std::string& error) = 0;
    // Writes the heightmaps (Heightdata.con and the tiles' data) under `directory`, not into the level.
    virtual bool terrainExportHeightmaps(const std::string& directory, std::string& result) = 0;
    // The view's terrain brush tool: while it is on, the left button paints (the middle button, or
    // Shift with the left, is the brush's other action) and objects are not picked.
    virtual bool setTerrainTool(bool active, std::string& result) = 0;
    // The surrounding terrain: the sectors (a query), a surrounding tile made active or not (its saved
    // "modified" flag; undoable, applySector is the replay), a surrounding heightmap imported and the
    // primary tile's edges smoothed (each one undoable change).
    virtual bool terrainSectors(std::string& result) = 0;
    virtual bool terrainSetSector(int x, int z, bool active, std::string& result) = 0;
    virtual bool terrainApplySector(int x, int z, bool active, std::string& error) = 0;
    virtual bool terrainImportSurrounding(const std::string& path, std::string& result) = 0;
    virtual bool terrainSmoothEdges(int width, float amplitude, float falloff, std::string& result) = 0;
    // The sea level (the cluster's, saved in Heightdata.con): undoable; applySeaLevel is the replay.
    virtual bool terrainSetSeaLevel(float level, std::string& result) = 0;
    virtual bool terrainApplySeaLevel(float level, std::string& error) = 0;
    // Undergrowth (App/EditSessionUndergrowth.cpp): the density map and its materials; the view's tool, the
    // material it paints and its radius; a script's dab (one undoable edit); the history's replay.
    virtual bool undergrowthInfo(std::string& result) = 0;
    virtual bool setUndergrowthTool(bool active, std::string& result) = 0;
    virtual bool setUndergrowthMaterial(int id, std::string& result) = 0;
    virtual bool setUndergrowthRadius(float radius, std::string& result) = 0;
    virtual bool undergrowthDab(float x, float z, bool erase, std::string& result) = 0;
    virtual bool applyUndergrowthChange(int id, const std::string& state, std::string& error) = 0;
    virtual bool setUndergrowthSetting(const std::string& key, float value, std::string& result) = 0;
    virtual bool setUndergrowthMaterialHeight(int id, float value, std::string& result) = 0;
    virtual bool setUndergrowthTypeProperty(int id, const std::string& type, const std::string& key,
                                            const std::string& value, std::string& result) = 0;
    virtual bool applyUndergrowthConfig(int id, const std::string& state, std::string& error) = 0;
    // Object bundles, groups and level brushes (App/EditSessionLibrary.cpp): the library folder, bundles
    // saved from the selection and placed (one undoable edit, grouped when setBundleGroup is on), the
    // selection's group, and the brushes - their edits write their files, each stamp or erase is undoable.
    virtual bool libraryInfo(std::string& result) = 0;
    virtual bool setLibraryFolder(const std::string& folder, std::string& result) = 0;
    virtual bool listBundles(std::string& result) = 0;
    virtual bool bundleInfo(const std::string& name, std::string& result) = 0;
    virtual bool saveBundle(const std::string& name, bool overwrite, std::string& result) = 0;
    virtual bool placeBundle(const std::string& name, const dice::hfe::Vec3& at, float yaw, std::string& result) = 0;
    virtual bool placeBundleHere(const std::string& name, float yaw, std::string& result) = 0;
    virtual bool deleteBundle(const std::string& name, std::string& result) = 0;
    virtual bool setBundleGroup(bool group, std::string& result) = 0;
    virtual bool groupSelection(std::string& result) = 0;
    virtual bool ungroupSelection(std::string& result) = 0;
    virtual bool selectGroup(std::string& result) = 0;
    virtual bool listBrushes(std::string& result) = 0;
    virtual bool brushInfo(const std::string& name, std::string& result) = 0;
    virtual bool selectBrush(const std::string& name, std::string& result) = 0;
    virtual bool defineBrush(const std::string& name, std::string& result) = 0;
    virtual bool addSelectionToBrush(std::string& result) = 0;
    virtual bool addBrushItem(const std::string& templateName, std::string& result) = 0;
    virtual bool setBrushProperty(const std::string& key, const std::string& value, std::string& result) = 0;
    virtual bool setBrushItem(int index, const std::string& key, const std::string& value, std::string& result) = 0;
    virtual bool removeBrushItem(int index, std::string& result) = 0;
    virtual bool deleteBrush(const std::string& name, std::string& result) = 0;
    virtual bool seedBrush(unsigned seed, std::string& result) = 0;
    virtual bool setBrushTool(bool active, std::string& result) = 0;
    virtual bool setBrushEraseRadius(float radius, std::string& result) = 0;
    virtual bool brushStamp(const dice::hfe::Vec3& at, std::string& result) = 0;
    virtual bool brushErase(const dice::hfe::Vec3& at, std::string& result) = 0;
    // Object templates (WP7, App/EditSessionTemplates.cpp): a template's files, its lines, a property set
    // or a line removed in place (undoable; applyObjectTemplateEdit is the replay), the files saved.
    virtual bool objectTemplateFiles(const std::string& name, std::string& result) = 0;
    virtual bool objectTemplateLines(const std::string& name, std::string& result) = 0;
    virtual bool objectTemplateEngineValue(const std::string& name, const std::string& property, std::string& result) = 0;
    virtual bool setObjectTemplateProperty(const std::string& name, const std::string& property,
                                           const std::string& value, std::string& result) = 0;
    virtual bool removeObjectTemplateProperty(const std::string& name, const std::string& property,
                                              std::string& result) = 0;
    virtual bool applyObjectTemplateEdit(int id, const std::string& state, std::string& error) = 0;
    virtual bool saveObjectTemplates(std::string& result) = 0;
    virtual bool objectTemplateChildren(const std::string& name, std::string& result) = 0;
    virtual bool addTemplateChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position,
                                  std::string& result) = 0;
    virtual bool moveTemplateChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position,
                                   const dice::hfe::Vec3& rotation, std::string& result) = 0;
    virtual bool removeTemplateChild(const std::string& name, const std::string& child, std::string& result) = 0;
    virtual bool objectTemplateComponents(const std::string& name, std::string& result) = 0;
    virtual bool objectTemplateFixes(const std::string& name, std::string& result) = 0;
    virtual bool showObjectTemplate(const std::string& name, std::string& result) = 0;
    virtual bool newTemplate(const std::string& className, const std::string& name, const std::string& folder,
                             std::string& result) = 0;
    virtual bool inheritTemplateSettings(const std::string& target, const std::string& source, std::string& result) = 0;
    virtual bool renameObjectTemplate(const std::string& name, const std::string& newName, std::string& result) = 0;
    virtual bool wizardCommand(const std::string& action, const std::string& argument, std::string& result) = 0;
    virtual bool changeTemplateType(const std::string& name, const std::string& className, std::string& result) = 0;
    virtual bool cloneTemplate(const std::string& source, const std::string& name, const std::string& folder,
                               std::string& result) = 0;
    virtual bool newTemplateChild(const std::string& parent, const std::string& className, const std::string& name,
                                  const dice::hfe::Vec3& position, std::string& result) = 0;
    virtual bool previewEffect(const std::string& templateName, std::string& result) = 0;
    virtual bool bakeGroundHemi(int resolution, const dice::hfe::Vec3& position, float size, std::string& result) = 0;
    virtual bool captureMinimap(int resolution, std::string& result) = 0;
    virtual bool generateEnvMaps(std::string& result) = 0;
    // Terrain lightmaps (Compile > Generate lightmaps): `quality` 0..3 is the retail dialog's resolution (0.25, 0.5, 1, 2),
    // `parts` primary, secondary or both, `options` - or nearest, resume, nearest+resume.
    virtual bool generateLightmaps(int quality, const std::string& parts, const std::string& options,
                                   std::string& result) = 0;
    // Roads and splines (App/EditSessionRoads.cpp): `object` EditorRoad for the editor's own commands, or a retail row's
    // object (SplineManager, Road, RoadTemplate, RoadTemplateTexture) with its method and arguments.
    virtual bool roadCommand(const std::string& object, const std::string& method, const std::vector<std::string>& args,
                             std::string& result) = 0;
    virtual bool effectAction(const std::string& action, std::string& result) = 0;
    virtual bool clearEffectPreview(std::string& result) = 0;
    virtual bool effectInfo(std::string& result) = 0;
    virtual bool objectTemplateComponentClasses(std::string& result) = 0;
    virtual bool addTemplateComponent(const std::string& name, const std::string& component, std::string& result) = 0;
    virtual bool removeTemplateComponent(const std::string& name, const std::string& component, std::string& result) = 0;
    virtual bool listModsCommand(std::string& result) = 0;
    virtual bool createModCommand(const std::string& name, std::string& result) = 0;
    virtual bool openMod(const std::string& name, std::string& result) = 0;
    virtual bool objectArchivesStatus(std::string& result) = 0;
    virtual bool extractObjectArchives(std::string& result) = 0;
    virtual bool packObjectArchives(const std::string& modFolder, std::string& result) = 0;
    // Anchor points (App/EditSessionAnchors.cpp): the primary object's anchor as text, the stamp, Align
    // and Raise (each one undoable edit), and the rotate gizmo's pivot.
    virtual bool anchorInfo(std::string& result) = 0;
    virtual bool setAlignStamp(std::string& result) = 0;
    virtual bool alignToStamp(std::string& result) = 0;
    virtual bool raiseByAnchor(std::string& result) = 0;
    virtual bool setGizmoPivot(int pivot, std::string& result) = 0;
    virtual bool setAnchor(const dice::hfe::Vec3& anchor, std::string& result) = 0;
    virtual bool anchorToBox(const std::string& which, std::string& result) = 0;
    virtual bool resetAnchor(std::string& result) = 0;
    virtual bool setAnchorMoveMode(bool on, std::string& result) = 0;
};

// Registers the editor's console objects; they act on `commands` until unregisterEditorConsole.
bool registerEditorConsole(IEditorCommands* commands);
void unregisterEditorConsole();

// EditorTest.* (App/TestHarness): scripted mouse and keyboard input and pauses, for UI tests. Only the
// interactive run sets one; without it the bindings are inactive and the console refuses them.
class ITestHarness
{
public:
    virtual ~ITestHarness() {}
    virtual bool wait(int frames, std::string& error) = 0;
    virtual bool mouseMove(float x, float y, std::string& error) = 0;
    virtual bool mouseBy(float dx, float dy, std::string& error) = 0;
    virtual bool mouseToObject(unsigned id, std::string& result) = 0;
    // The mouse to the mark of the template preview's part `index` (EditSession::previewParts).
    virtual bool mouseToPart(int index, std::string& result) = 0;
    virtual bool mouseToAxis(int axis, float pixels, std::string& result) = 0;
    virtual bool mouseButton(int button, bool down, std::string& error) = 0;
    virtual bool mouseWheel(float steps, std::string& error) = 0;
    virtual bool key(const std::string& name, bool down, std::string& error) = 0;
    virtual bool dragTemplate(const std::string& path, std::string& error) = 0;
    // The mouse to a combat area's point (its handle a metre above the terrain).
    virtual bool mouseToAreaPoint(const std::string& area, int index, std::string& result) = 0;
    // The mouse to the middle of the edge from point `index` to the next.
    virtual bool mouseToAreaEdge(const std::string& area, int index, std::string& result) = 0;
    // The mouse to an ambient sound's marker.
    virtual bool mouseToSound(const std::string& name, std::string& result) = 0;
    // The mouse to the terrain at world (x, z).
    virtual bool mouseToGround(float x, float z, std::string& result) = 0;
    // The mouse to where world (x, z) is drawn on the Minimap panel.
    virtual bool mouseToMinimap(float x, float z, std::string& result) = 0;
    // An ImGui item by its label as last drawn: "<label>", "<label>@<n>" for the n-th such item (from 0, in draw
    // order), "<window>|<label>" for one in a window whose name starts so.
    virtual bool mouseToItem(const std::string& item, std::string& result) = 0;
    // Characters typed into the item that has the keyboard (a text field), as a hand would.
    virtual bool typeText(const std::string& text, std::string& error) = 0;
};
void setTestHarness(ITestHarness* harness);

// EditorPlayTest.* (App/PlayTest): soldier mode - soldier, leave, info, kit <team> <kit>, hold <input> <value> <steps>,
// drive (the selected spawner's vehicle), driveTemplate <template>.
// Set while a level is open; without it the rows refuse.
class IPlayTestCommands
{
public:
    virtual ~IPlayTestCommands() {}
    virtual bool playTestActive() const = 0;
    virtual bool enterSoldierMode(std::string& result) = 0;
    virtual bool leaveSoldierMode(std::string& result) = 0;
    virtual bool setPlayTestKit(int team, int kit, std::string& result) = 0;
    virtual bool holdPlayTestInput(const std::string& name, float value, int steps, std::string& result) = 0;
    // An empty template name drives the selected object spawner's vehicle.
    virtual bool drivePlayTestVehicle(const std::string& templateName, std::string& result) = 0;
    // A weapon template tested: the soldier holding it (EditorPlayTest.weapon).
    virtual bool testPlayTestWeapon(const std::string& templateName, std::string& result) = 0;
    virtual std::string describePlayTest() const = 0;
};
void setPlayTest(IPlayTestCommands* playTest);
// Whether soldier mode is on (the shell's Camera menu ticks it).
bool playTestActive();

// EditorHud.* (App/HudEditor, added): the game's HUD in the editor (+hud 1) and its HUD editor - info, preview <0|1>
// (the HUD drawn without the play test), set <variable> <value> (a HUD variable held while previewing, "-" lets it
// go), select <node> (the part the editor shows, what shows it held), node <node>, rect <node> <x/y/w/h>,
// setRow <node> <n> <words>, addRow <node> <method> <words>, removeRow <node> <n>, changes, and the history's own
// setLine|insertLine|removeLine <script> <line> [text, percent-encoded]. Set while the HUD is up.
class IHudCommands
{
public:
    virtual ~IHudCommands() {}
    virtual std::string describeHud() const = 0;
    virtual bool setHudPreview(bool on, std::string& result) = 0;
    virtual bool setHudVariable(const std::string& name, const std::string& value, std::string& result) = 0;
    virtual bool selectHudNode(const std::string& node, std::string& result) = 0;
    virtual std::string describeHudNode(const std::string& node) const = 0;
    // op 0 sets a line, 1 inserts one before `line`, 2 removes it.
    virtual bool editHudLine(int op, int script, int line, const std::string& text, std::string& result) = 0;
    virtual bool setHudRect(const std::string& node, int x, int y, int width, int height, std::string& result) = 0;
    virtual bool setHudRow(const std::string& node, int row, const std::string& words, std::string& result) = 0;
    virtual bool addHudRow(const std::string& node, const std::string& method, const std::string& words,
                           std::string& result) = 0;
    virtual bool removeHudRow(const std::string& node, int row, std::string& result) = 0;
    virtual std::string describeHudChanges() const = 0;
    // Where a part's outline stands in the window (pixels): its centre (0) or its size handle (1) - the test harness's
    // EditorTest.mouseToHud. False with `error` when the part has no outline.
    virtual bool hudScreenPoint(const std::string& node, int which, float& x, float& y, std::string& error) const = 0;
};
void setHudCommands(IHudCommands* hud);

// EditorView.panel <name> <0|1> opens or closes a panel through this handler (the shell owns the
// panels, not the edit session). False from the handler: no panel of that name.
typedef bool (*PanelHandler)(void* context, const std::string& name, bool visible);
void setPanelHandler(PanelHandler handler, void* context);
// The view's own switches (EditorView.pauseRenderer <0|1>, renderOneFrame): "pause", "resume", "one".
typedef bool (*ViewHandler)(void* context, const std::string& action);
void setViewHandler(ViewHandler handler, void* context);
// The shell's custom commands (EditorTools.*): action "list", "setCommand", "setFile", "runCommand", "runFile"
// with the slot (1..3) and, for the setters, the text.
typedef bool (*ToolsHandler)(void* context, const std::string& action, int index, const std::string& argument,
                             std::string& result);
void setToolsHandler(ToolsHandler handler, void* context);

// Runs console lines through the engine console with full access, one line at a time. Stops at the
// first line that fails - an editor command that reports an error, or any line that answers with
// text (a successful set answers nothing) - and says which. This is CommandHistory's runner.
bool runConsoleScript(const std::string& script, std::string& error);
bool consoleScriptRunner(void* context, const std::string& script, std::string& error);

// One line typed by the user (the Console panel, +runConFile).
struct ConsoleLineResult
{
    std::string output;         // the console's answer, possibly empty
    bool        failed;         // an editor command reported an error
    // The line may have changed the level in a way the undo history cannot take back: anything but
    // an editor command or a property query (a set/get binding given no argument). §4: the history
    // is then cleared.
    bool        unknownEffect;
    ConsoleLineResult() : failed(false), unknownEffect(false) {}
};
ConsoleLineResult runConsoleLine(const std::string& line);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_EDITORCONSOLE_H
