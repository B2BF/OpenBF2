// Code/Tools/BF2Editor/App/EditSession.h

// The editing state of the open level (WP5, milestone E4): the selection, the undo history, picking
// and the object transform edits. It implements the editor's console commands
// (Framework/EditorConsole.h), so the viewport, the panels, the menus and a script all make the same
// edits, and every edit reaches the level as console lines recorded in the history.

// Objects are named by their engine id (IObject +0x10) and looked up through
// ObjectManager::getRegisteredObject (+0xB0) each time; nothing here holds an object pointer across
// frames. Editor-only code (D3).
#ifndef BF2EDITOR_APP_EDITSESSION_H
#define BF2EDITOR_APP_EDITSESSION_H

#include "Framework/EditorConfig.h"

#include "Framework/CommandHistory.h"
#include "Framework/EditorConsole.h"
#include "Framework/LayerSummary.h"
#include "Framework/LevelObjectFile.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SavePlan.h"
#include "Framework/UndergrowthConfig.h"
#include "Framework/UndergrowthMap.h"
#include "Framework/Selection.h"
#include "App/AnimationDocuments.h"
#include "App/DescriptionDocuments.h"
#include "App/GameplayLayerModel.h"
#include "Framework/LightSetupFile.h"
#include "App/EditableTerrain.h"
#include "Framework/LevelEditorDefs.h"
#include "App/MaterialDocuments.h"
#include "App/ObjectWizard.h"
#include "App/RoadNetwork.h"
#include "App/TemplateDocuments.h"
#include "App/TerrainEditing.h"

#include "Math/Mat4.h"
#include "Math/Vec3.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace dice { namespace hfe { namespace world { class IObject; class IObjectTemplate; } } }

namespace bf2editor
{

class EditorLevel;
class FreeCamera;
class LevelDocument;
struct ObjectPlacement;

// The per-object properties the level files keep after the layer (Framework/LevelObjectFile), as the
// engine holds them (IObject's getters), and a spawner's control point (IObjectSpawner, phase C: the
// gameplay layers' `Object.setControlPointId`, written unless it is -1 as ObjectSpawner::makeScript does).
struct ObjectProperties
{
    int  group;
    bool disableChildren;
    bool notInAI;
    bool isOvergrowth;
    int  lightSourceMask;
    bool isSpawner;
    int  controlPointId;
    ObjectProperties()
        : group(0), disableChildren(false), notInAI(false), isOvergrowth(false), lightSourceMask(0), isSpawner(false),
          controlPointId(-1)
    {
    }
};

enum class GizmoMode
{
    Select = 0,     // no gizmo; clicks select
    Move = 1,
    Rotate = 2
};

enum class GizmoSpace
{
    World = 0,
    Local = 1
};

// What a click does to the selection: replace it (a miss clears it), add to it, or toggle the object
// in it (a miss changes nothing either way).
enum class PickMode
{
    Set,
    Add,
    Toggle
};

// A document the level's save writes besides the level's own files (the HUD editor's scripts, App/HudEditor).
class ISavedDocument
{
public:
    virtual ~ISavedDocument() {}
    // Its part of the save when it has changes, planned with the level's other parts.
    virtual void planSave(SavePlan& plan) = 0;
    // What the unsaved-changes summary says about it: a sentence after a blank, or "" when nothing changed.
    virtual std::string unsavedText() const = 0;
};

class EditSession : public IEditorCommands
{
public:
    EditSession(EditorLevel& level, FreeCamera& camera);

    // The 3D view in window pixels. The render viewport is the whole client area (FreeCamera::apply),
    // with the panels drawn over it.
    void setViewSize(int width, int height);
    int viewWidth() const { return m_width; }
    int viewHeight() const { return m_height; }

    CommandHistory& history() { return m_history; }
    const CommandHistory& history() const { return m_history; }
    Selection& selection() { return m_selection; }
    const Selection& selection() const { return m_selection; }
    GizmoMode gizmoMode() const { return m_mode; }
    GizmoSpace gizmoSpace() const { return m_space; }
    void setMode(GizmoMode mode) { m_mode = mode; }
    void setSpace(GizmoSpace space) { m_space = space; }

    // Engine lookups
    static dice::hfe::world::IObject* object(unsigned id);
    static bool transformOf(unsigned id, dice::hfe::Mat4& out);
    // "dock_crane #1234", or "#1234 (not in the level)".
    static std::string describe(unsigned id);
    // Euler angles as .con's Object.rotation reads and writes them: yaw / pitch / roll in degrees
    // (Scene/Object/ObjectManagerConsole.cpp). setRotation keeps the matrix's translation and scale.
    static dice::hfe::Vec3 rotationOf(const dice::hfe::Mat4& m);
    static void setRotation(dice::hfe::Mat4& m, const dice::hfe::Vec3& yawPitchRoll);
    static ObjectProperties propertiesOf(dice::hfe::world::IObject* object);
    // The lines that give object #id these properties (EditorObject.activate, then the Object.* lines).
    static std::string propertiesScript(unsigned id, const ObjectProperties& properties);

    // The root object under window pixel (x, y), or 0; `detail` says what the ray met. Objects are
    // tested on their collision meshes (ObjectManager::intersectLine), the terrain by stepping along
    // the ray over the primary heightmap, and whichever is nearer wins.
    unsigned pick(float x, float y, std::string& detail) const;
    // The surface point under window pixel (x, y): the nearer of the collision surface the pick ray
    // meets (hidden and locked objects left out) and the terrain; false when it meets neither.
    // With `ignoreSelection`, the selected objects are looked through (a drag slides them over what is
    // under them).
    bool surfaceAt(float x, float y, float out[3], std::string& detail, bool ignoreSelection = false) const;
    // The terrain height under (x, z) (EditorLevel::groundHeight); false off the heightmap.
    bool groundHeight(float x, float z, float& height) const;
    // pick() applied to the selection; `result` says what happened. Locked layers are skipped by pick.
    void pickInto(float x, float y, PickMode mode, std::string& result);

    // One undoable edit of object transforms: the history runs, for each change,
    //     EditorObject.activate <id>
    //     Object.absoluteTransformation [..][..][..][..]
    // and afterwards each object must hold its new matrix, or the edit is reported as failed.
    struct TransformChange
    {
        unsigned        id;
        dice::hfe::Mat4 before;
        dice::hfe::Mat4 after;
    };
    bool applyTransforms(const std::vector<TransformChange>& changes, const std::string& label,
                         std::string& error);

    // Shared by the Edit menu, the shortcuts and the History panel: run the console command and log
    // what happened.
    void runUndo();
    void runRedo();
    // File > Save and Ctrl+S: with the preview option on the shell shows the save first (takeSaveRequest, App/
    // SavePreviewPanel), else the level is saved at once.
    void runSave();
    void setPreviewBeforeSave(bool on) { m_previewBeforeSave = on; }
    bool takeSaveRequest()
    {
        const bool requested = m_saveRequested;
        m_saveRequested = false;
        return requested;
    }

    // The level's object files (WP6), opened after the level loads; 0 when they could not be (the log
    // says why), and then nothing can be saved.
    // The level has loaded: the terrain as it stands is the saved state (dirty checks compare with it).
    void setDocument(LevelDocument* document)
    {
        dropPendingSave();
        m_document = document;
        m_terrain.markSaved();
        m_verticalScaleAtLoad = m_terrain.verticalScale();
        adoptLayerModel();
    }
    // The retail layer model's view flags and active layer once the document has opened (E14).
    void adoptLayerModel();

    // Terrain (parity phase E, App/TerrainEditing)
    TerrainEditing& terrain() { return m_terrain; }
    TerrainBrushSettings& terrainBrush() { return m_terrainBrush; }
    // A stroke from the view: begin once, one tick per frame while a button is held, end on release.
    // The end records one undoable edit when anything moved.
    bool beginTerrainStroke(std::string& error);
    bool terrainStrokeTick(float x, float z, bool secondary, float frameTime, std::string& error);
    void endTerrainStroke();
    bool terrainStrokeActive() const { return m_terrain.strokeActive() || m_paintStroke; }
    // The view's brush tool (ViewportTools paints while it is on; picking is off). Turning it off ends
    // a stroke under way.
    bool terrainToolActive() const { return m_terrainTool; }
    void setTerrainToolActive(bool active);
    // The terrain under window pixel (x, y), objects looked through (the retail plugin's cursor: a pick
    // of the terrain alone); false when the ray meets no ground.
    bool terrainUnderPixel(float x, float y, float out[3]) const;
    LevelDocument* document() const { return m_document; }
    // One line for the close prompt: what is unsaved.
    std::string unsavedSummary();
    // Whether a save would write anything (or something changed that cannot be saved): edits that were
    // undone, or that cancel out, leave nothing to ask about.
    bool hasUnsavedChanges();

    // Layers and filters (WP5 part 2)
    // A layer is the number every placed object carries (IObject::getLayer, the .con's
    // `Object.layer n`). Hiding and locking are the editor'S view, never saved with the level:
    //   - a hidden layer's root objects have their draw flag cleared (IObject::OFIsVisible, via
    //     updateFlags) and get it back when shown. Only objects the editor hid are shown again. The
    //     static shadow pass does not test the flag, so a hidden static can still cast a shadow;
    //   - a locked layer's objects cannot be picked.
    // Classes (the template class, ObjectTemplate.create's vocabulary) can be made unpickable the same
    // way. Counts are over the root objects in the grid (ObjectManager::getAllObjectsFromGrid).
    struct LayerRow
    {
        int  layer;
        int  objects;
        bool hidden;
        bool locked;
    };
    struct ClassRow
    {
        unsigned    classId;
        std::string name;
        int         objects;
        bool        pickable;
    };
    const std::vector<LayerRow>& layerRows();
    const std::vector<ClassRow>& classRows();
    void markCountsStale() { m_countsStale = true; }
    bool isLayerHidden(int layer) const { return m_hiddenLayers.count(layer) != 0; }
    bool isLayerLocked(int layer) const { return m_lockedLayers.count(layer) != 0; }
    bool isClassPickable(unsigned classId) const { return m_unpickableClasses.count(classId) == 0; }
    void setClassPickable(unsigned classId, bool pickable);
    // Picking's filter on a root object: drawn, its layer not locked, its class pickable.
    bool canPick(dice::hfe::world::IObject* root) const;
    // The template class name ("SimpleObject", "Bundle", ...) or "UnknownTemplateClass".
    static std::string className(unsigned classId);
    static int layerOf(dice::hfe::world::IObject* object);     // the root's layer

    // Called once a frame before the UI: re-applies hidden layers after an edit changed layers.
    void update();

    // IEditorCommands
    bool activateObject(unsigned id, std::string& error);
    bool selectObject(unsigned id, bool add, std::string& error);
    bool resolveObject(const std::string& reference, unsigned& id, std::string& error);
    bool pickAt(float x, float y, std::string& result);
    bool pickAdd(float x, float y, std::string& result);
    bool selectTemplate(const std::string& name, std::string& result);
    bool selectionInfo(std::string& result);
    bool translateSelection(const dice::hfe::Vec3& delta, std::string& error);
    bool rotateSelection(const dice::hfe::Vec3& yawPitchRollDelta, std::string& error);
    bool setSelectionPosition(const dice::hfe::Vec3& position, std::string& error);
    bool setSelectionRotation(const dice::hfe::Vec3& yawPitchRoll, std::string& error);
    bool undo(std::string& error);
    bool redo(std::string& error);
    void clearHistory(const std::string& reason);
    bool frameSelection(std::string& error);
    bool setGizmoMode(int mode, std::string& error);
    bool setGizmoSpace(int space, std::string& error);
    bool setLayerHidden(int layer, bool hidden, std::string& error);
    bool setLayerLocked(int layer, bool locked, std::string& error);
    bool selectLayer(int layer, std::string& error);
    bool setSelectionLayer(int layer, std::string& error);
    bool levelStatus(std::string& result);
    bool saveLevel(std::string& result);
    // File > Resave: the save, with every level file written even when unchanged.
    bool resaveLevel(std::string& result);
    // The save preview, backups and restores (App/EditSessionSave.cpp; [CHOSEN, not recovered], issue draft 13).
    // Every save is planned before anything is written (Framework/SavePlan) and written as a whole, each file it
    // replaces kept in the level's backup folder (<game folder>/bf2editor/Backups/<mod>/<level>/<date and time>).
    // previewSave plans the level save without writing anything and keeps it as the pending save; `result` lists
    // its files. writePendingSave writes it when nothing changed since (else it is dropped and nothing written).
    bool previewSave(bool everything, std::string& result);
    const SavePlan* pendingSave() const { return m_pendingSave.get(); }
    bool writePendingSave(std::string& result);
    void dropPendingSave() { m_pendingSave.reset(); }
    // A pending file (counted over the parts that are saved, in order) against the file on disk: a unified diff of a
    // text file, the sizes of another.
    bool pendingSaveDiff(size_t index, std::string& result);
    // The level's backups, newest first; one put back with every save after it (by its name, or "#n" for the n-th
    // newest), which needs every change saved first and starts the editor again on the level.
    std::vector<SaveBackup> backups() const;
    bool listBackups(std::string& result);
    bool restoreBackup(const std::string& name, std::string& result);
    // The level's backup folder, and how many backups it keeps (0: every one; the editor's options set it).
    std::string backupFolder() const;
    void setBackupsKept(int count) { m_backupsKept = count < 0 ? 0 : count; }
    // The HUD editor's scripts, saved with the level (0: none).
    void setHudDocument(ISavedDocument* document) { m_hudDocument = document; }
    // The open level's mod folder as the engine names it ("mods/bf2"); "" with no level.
    std::string modFolder() const;
    int backupsKept() const { return m_backupsKept; }
    // Files a command writes at once, outside the level save (a light setup, the object lists, a clone, a copied
    // layer), as one backed-up save labelled `label`: {path, content} each, written all or none. False with `error`
    // when nothing could be written or source control refused one.
    bool writeFilesNow(const std::string& label, const std::vector<std::pair<std::string, std::string> >& files,
                       std::string& error);
    // The console's rows for the above (EditorLevel.preview, previewAll, previewDiff, writePreview, dropPreview,
    // backups, restore): `action` preview | previewAll | diff <n> | write | drop | backups | restore <name>.
    bool saveCommand(const std::string& action, const std::string& argument, std::string& result);
    bool packLevel(const std::string& modFolder, std::string& result);
    bool saveLevelAs(const std::string& name, bool open, std::string& result);
    bool createLevel(const std::string& name, const std::string& terrain, bool open, std::string& result);
    // Objects made and unmade (WP6). createObject and deleteSelection are the user's edits; the
    // history's scripts replay them with recreateObject and removeObject.
    bool createObject(const std::string& templateName, const dice::hfe::Vec3& position,
                      const dice::hfe::Vec3& yawPitchRoll, int layer, std::string& result);
    bool deleteSelection(std::string& result);
    bool recreateObject(unsigned originalId, const std::string& templateName, const dice::hfe::Mat4& transform,
                        int layer, std::string& error);
    bool removeObject(unsigned id, std::string& error);
    // Placement (WP6 part 2). New objects and copies go into StaticObjects.con on layer 1 (placeAt) or
    // the original's layer (duplicate); objects of the other files cannot be copied, since each has a
    // template of its own there. Snapping sets each object's position on the terrain under it.
    bool placeObjectAt(const std::string& templateName, float x, float y, std::string& result);
    // createObject, the object then set to stand on `standOn` (a surface height) by the retail snap.
    bool createObjectOn(const std::string& templateName, const dice::hfe::Vec3& position,
                        const dice::hfe::Vec3& yawPitchRoll, int layer, const float* standOn, std::string& result);
    bool duplicateSelection(std::string& result);
    bool snapSelectionToGround(std::string& result);
    // Phase A. Box selection tests each pickable root object's position projected into the
    // view. Replace, paste and duplicate make objects in StaticObjects.con only (see duplicate).
    bool selectInBox(float x0, float y0, float x1, float y1, int mode, std::string& result);
    bool replaceSelection(const std::string& templateName, std::string& result);
    bool copyTransform(std::string& result);
    bool pasteTransform(std::string& result);
    bool snapSelectionToGrid(float step, std::string& result);
    bool copySelection(std::string& result);
    bool cutSelection(std::string& result);
    bool pasteClipboard(const dice::hfe::Vec3& at, bool atGiven, std::string& result);
    // The grid the Edit menu's snap uses (the gizmo's move snap, set by ViewportTools each frame).
    float gridStep() const { return m_gridStep; }
    // The camera the session edits through (its mode and position for the menus).
    const FreeCamera& camera() const { return m_camera; }
    // The retail host's camera position bookmarks (App/EditSessionHost.cpp): "add" (two at most, the oldest
    // dropped), "toggle" (to the farther of the two), "go" <1|2>, "list". Kept for the session.
    struct CameraBookmark
    {
        float x, y, z, yaw, pitch;
    };
    bool cameraBookmark(const std::string& action, int index, std::string& result);
    const std::vector<CameraBookmark>& cameraBookmarks() const { return m_cameraBookmarks; }
    bool setCameraMode(int mode, std::string& result);
    bool setCameraPosition(const dice::hfe::Vec3& position, std::string& result);
    bool resetCameraPosition(std::string& result);
    // A speed at or below 0 resets it.
    bool setCameraSpeed(float metresPerSecond, std::string& result);
    bool cameraInfo(std::string& result);
    bool setSelectionProperty(const std::string& name, const std::string& value, std::string& result);

    // Markers (phase C)
    // Objects of these template classes have no geometry to see or click, so the view draws a marker
    // for each and a click near one picks it. 0 when the class gets no marker.
    enum MarkerKind { MarkerNone = 0, MarkerSpawner, MarkerSpawnPoint, MarkerControlPoint, MarkerTrigger, MarkerEffectArea };
    MarkerKind markerKind(dice::hfe::world::IObject* object) const;
    static const char* markerName(MarkerKind kind);
    bool markersShown() const { return m_markersShown; }
    bool setMarkersShown(bool shown, std::string& error);
    // Whether a kind's markers are drawn and picked: the markers on, and the kind's overlay filter on.
    bool markerShown(MarkerKind kind) const;

    // Snap modes
    // What a free move (the move gizmo's centre slid over the ground) does with each object: onto the ground
    // (the terrain or the water, whichever is higher; on by default, as retail's), onto other objects' tops
    // (a ray straight down onto their collision meshes), and X/Z onto the heightmap's grid or its half.
    struct SnapModes
    {
        bool ground;
        bool objects;
        int  grid;              // 0 off, 1 the heightmap's grid, 2 its half
        SnapModes() : ground(true), objects(false), grid(0) {}
    };
    const SnapModes& snapModes() const { return m_snapModes; }
    bool setSnapMode(const std::string& name, const std::string& value, std::string& result);
    std::string snapList() const;
    void snapGrid(float& x, float& z) const;
    // The height a free move sets an object down on at (x, z): the terrain or the water, or another object's
    // top when snapping onto objects is on (the selection itself left out). False off the terrain.
    bool snapSurface(float x, float z, float& height) const;

    // View overlays
    // What the view draws besides the level: the gameplay markers by kind, the lines from spawn points and
    // spawners to their control point, a control point's radius on the ground, the sea-level plane, the
    // selection's bounding spheres, light source markers, and the ambient sounds' markers and reach. Kept for
    // the session; EditorView.overlay <name> <value> sets one.
    struct OverlayOptions
    {
        bool controlPoints, spawnPoints, spawners, ambient;     // the gameplay markers, by kind
        bool connections, controlPointRadius, seaLevel, boundingSpheres, lightSources;
        bool soundReachOnGround;    // the reach as an area on the ground, not a sphere at the sound's height
        bool selectionBoxes;        // the selection's bounding boxes (retail's selection display 1; 0 draws none)
        bool hoverBox;              // a yellow box round the object under the cursor (retail's Draw Selection Box)
        bool supplyLines;           // control points of one supply group joined, and each one's lines to the groups
                                    // it needs (team 1 red, team 2 blue; retail's SetDrawSupplyLine)
        int  soundMarkers;          // 0 off, 1 small, 2 medium, 3 large
        int  soundReach;            // 0 none, 1 the selected sound's, 2 every sound's
        OverlayOptions();
    };
    const OverlayOptions& overlays() const { return m_overlays; }
    bool setOverlay(const std::string& name, const std::string& value, std::string& result);
    std::string overlayList() const;
    // Each spawn point's and spawner's line to its control point (its template's setControlPointId, or
    // the spawner object's), and each control point's radius (its template's); read from the level files
    // again after an edit.
    // kind: 0 a spawn point's or spawner's line to its control point, 1 two control points of one supply group,
    // 2 / 3 a control point's line to a group team 1 / team 2 needs.
    struct OverlayLink { unsigned from; unsigned to; int kind = 0; };
    struct OverlayCircle { unsigned id; float radius; };
    void overlayGeometry(std::vector<OverlayLink>& links, std::vector<OverlayCircle>& circles);

    // Templates the level files define (phase C)
    bool setTemplateProperty(const std::string& name, const std::string& property, const std::string& value,
                             std::string& result);
    bool applyTemplateProperty(const std::string& name, const std::string& property, const std::string& value,
                               std::string& error);
    bool removeTemplateLine(const std::string& name, const std::string& key, std::string& error);
    bool templateInfo(const std::string& name, std::string& result);
    // Gameplay and ambient objects (phase C)
    // A new object with a template of its own named `name` (the name must be new): an object spawner,
    // spawn point or control point in the loaded gameplay layer, a trigger or an effect area in
    // AmbientObjects.con - the lines the stock files give every template of the kind, then the object on the layer the file's objects have. Spawners and spawn points belong to the
    // nearest control point, an effect area to the nearest trigger; a control point or trigger gets a new
    // id. An effect area needs its effect, a Bundle or EffectBundle template (createEffectArea). Undoable;
    // deleting the object (deleteSelection) takes its template out of the file with it.
    bool createGameplayObject(const std::string& kind, const std::string& name, const dice::hfe::Vec3& position,
                              std::string& result);
    bool createGameplayObjectHere(const std::string& kind, const std::string& name, std::string& result);
    bool createEffectArea(const std::string& name, const std::string& effect, const dice::hfe::Vec3& position,
                          std::string& result);
    bool createEffectAreaHere(const std::string& name, const std::string& effect, std::string& result);
    // A template a level file defines renamed, with its objects (App/EditSessionRename.cpp): the user's
    // edit, one undoable entry whose scripts are the replay both ways. The objects are made again from
    // the new template where they stood, with their properties, and every older id follows them
    // (resolve()). The engine keeps a template until the level loads again, so the old one stays behind
    // unused and a rename back uses it again. What players see (a control point's setControlPointName)
    // is a line of the template and does not change.
    bool renameLevelTemplate(const std::string& from, const std::string& to, std::string& result);
    bool applyTemplateRename(const std::string& from, const std::string& to, std::string& error);
    // Whether `name` can be an effect area's linked effect: the client's AmbientEffectArea constructor
    // stops the game on anything but a Bundle or EffectBundle template.
    static bool validEffectContainer(const std::string& name, std::string& error);
    // The level's objects of a class whose templates the level files define: their template, the id
    // their template gives under `idKey` (a lower-case line key; -1 when it gives none) and where the
    // object stands. controlPoints() is those of ControlPoint by "controlpointid".
    struct LevelObjectRow
    {
        std::string name;
        int         id;
        float       position[3];
        int         layer = 1;      // its object's Object.layer (layersMeet)
    };
    std::vector<LevelObjectRow> levelObjectsOf(const char* className, const char* idKey) const;
    std::vector<LevelObjectRow> controlPoints() const;
    // Supply lines and control-point groups: "supplyGroups" (the
    // level's), "groupControlPoints" / "ungroupControlPoints" (the selected ones), "connectSupply" <team supply
    // group>, "disconnectSupply" <group>, and the history's replays "applyGroup" <template id> and "applySupply"
    // <template team group needed add|remove>. Reached through hostTool.
    bool supplyCommand(const std::string& action, const std::string& argument, std::string& result);
    // The terrain and object lightmap light setups: "lightSetup" (the
    // current one), "lightSetupMode" <object|terrain>, "lightSetupSet" <setting value>, "lightSetupSave".
    bool lightSetupCommand(const std::string& action, const std::string& argument, std::string& result);
    int  lightSetupMode() const { return m_lightSetupMode; }
    // The retail layer model of the gameplay objects: "status", "list", "add" <name>, "rename" <id> <name>, "filters" <id> <modes> <players>, "disable"
    // <id> <0|1>, "active" <id>, "remove" <id> <move|delete> (its objects to the Default, or deleted; its combat
    // areas always deleted, as retail's), and the history's replay "applyLayers" <edit> <before|after>.
    bool layersCommand(const std::string& action, const std::vector<std::string>& args, std::string& result);
    GameplayLayerModel& layerModel() { return m_layerModel; }
    // The renderer's TerrainEditable when the level opened with +editableTerrain 1 (App/EditableTerrain).
    EditableTerrain& editableTerrain() { return m_editableTerrain; }
    // Ground properties (EditorGround.*, App/EditSessionTerrain.cpp): a setting as one undoable edit, its replay, a
    // layer taken out (an undone creation), the listing, and the save of Editor/GroundProperties.con.
    bool groundSet(int layer, const std::string& name, const std::string& value, std::string& result);
    bool groundApply(int layer, const std::string& name, const std::string& value, std::string& result);
    bool groundRemoveLayer(int layer, std::string& result);
    std::string groundListing() const;
    // Editor/GroundProperties.con, planned when the ground properties changed.
    void planGroundProperties(SavePlan& plan);
    // The painted terrain maps, planned into the level's Editor folder (EditableTerrain::saveMaps stages them).
    void planTerrainMaps(SavePlan& plan);
    // The compiled terrain (EditableTerrain::compileTerrain: terraindata.raw and the painted patches' game textures)
    // planned into the level, with the heightmaps when `heights` changed; nothing when neither the heights nor the
    // surface (`surface`: paint, ground properties) changed. Without the editable terrain changed heights are refused.
    void planCompiledTerrain(SavePlan& plan, bool heights, bool surface);
    // The level's overgrowth, regenerated and planned into its overgrowth folder (EditableTerrain::saveOvergrowth);
    // with `required` false a level without one plans nothing.
    void planOvergrowth(SavePlan& plan, bool required);
    // The batch save parts (+saveTerrainAndClose, +saveOvergrowthAndClose; both in +saveAndClose), written whether or
    // not anything changed, as the retail editor's part saves were: the terrain part saves changed ground properties
    // and painted maps, then compiles the terrain and saves it (with the heightmaps when they changed); the overgrowth
    // part regenerates and saves the level's overgrowth - with `required` false a level without one saves nothing.
    bool saveTerrainPart(std::string& result);
    bool saveOvergrowthPart(bool required, std::string& result);
    // Copies staged files ({staged path, path relative to the level folder}) that differ from the level's into the
    // level, as one save (backed up). False with `error` when they could not be written (then none is).
    bool copyStagedFiles(const std::vector<std::pair<std::string, std::string> >& files, int& written, std::string& error);
    // The same files added to a save part.
    void addStagedFiles(SavePart& part, const std::vector<std::pair<std::string, std::string> >& files) const;
    // The layer a new object of `file` takes: the active layer in the editor file, else the file's own.
    int newObjectLayer(size_t file) const;
    // Whether objects of the two Object.layer ids can be in one game-mode file: the same layer, or two whose filters
    // share a game mode and size (the Default has every filter). Every layer's objects are in the view at once, and
    // each game mode numbers its control points itself, so a control point id joins only objects whose layers meet.
    // True when the layer model is off (one game-mode file is loaded) or a layer is not one of the model's.
    bool layersMeet(int a, int b) const;
    struct SupplyPoint
    {
        std::string            name;        // the control point's template
        unsigned               object;      // its object in the level (0 when it has none)
        int                    layer = 1;   // its object's Object.layer
        int                    group;       // supplyGroupId, -1 none
        std::vector<int>       needs[2];    // the groups team 1 / team 2 need supplied
        std::vector<int>       needed[2];   // each one's third word (supplyGroupNeeded <team> <group> <needed>)
    };
    std::vector<SupplyPoint> supplyPoints() const;

    // Gameplay layers (phase C)
    // The level's layers on disk ("gpm_cq 16, gpm_cq 32, *gpm_cq 64" - the open one starred); a switch to
    // another, which the editor makes by starting again on it once the close prompt has saved or
    // discarded the changes (GameLogic loads one layer per level load, and the level is not unloaded in
    // place); and a copy of the open layer as a new one. A hidden or batch run does not relaunch.
    bool listLayers(std::string& result);
    bool switchLayer(const std::string& gameMode, int size, std::string& result);
    bool duplicateLayer(const std::string& gameMode, int size, std::string& result);
    void setCanRelaunch(bool canRelaunch) { m_canRelaunch = canRelaunch; }
    bool relaunchRequested() const { return !m_relaunchMode.empty(); }
    const std::string& relaunchMode() const { return m_relaunchMode; }
    int relaunchSize() const { return m_relaunchSize; }
    const std::string& relaunchLevel() const { return m_relaunchLevel; }     // "" for the same level
    const std::string& relaunchMod() const { return m_relaunchMod; }         // "" for the same mod
    void cancelRelaunch() { m_relaunchMode.clear(); m_relaunchLevel.clear(); m_relaunchMod.clear(); }
    // The mod manager (Framework/ModScaffold): the mods under mods/ (those with a Mod.desc), a new one made
    // with the retail scaffold over bf2, and another opened - the editor starts again on it, on the same
    // level, once the close prompt has saved or discarded the changes.
    bool listModsCommand(std::string& result);
    bool createModCommand(const std::string& name, std::string& result);
    bool openMod(const std::string& name, std::string& result);
    bool isOpenLayer(const std::string& gameMode, int size) const;

    // Comparing gameplay layers (phase C)
    // Every layer on disk summarised (Framework/LayerSummary): the open one as it is now, the others read
    // from their files, never run. One other layer can be shown beside the open one: its control points
    // and combat areas drawn as white outlines in the view and on the minimap.
    const std::vector<LayerSummary>& layerSummaries();
    bool layersOverview(std::string& result);
    bool setCompareLayer(const std::string& gameMode, int size, std::string& result);  // size 0: none
    const LayerSummary* compareLayer() const { return m_compareLayerSet ? &m_compareLayer : 0; }
    static const float kLayerMergeDistance;     // metres: control points this near are one place

    // Level settings (phase D)
    // Init.con, Sky.con and Water.con's settings (App/LevelSettings: which statement, what kind of value).
    // `set` is the undoable edit; `apply` its replay, which writes the file (and the editor-branch twin
    // where the file has one) and, for a live setting, first runs the engine's own line so the view shows
    // it; `unset` takes out a statement the edit added. The camera before spawning can be the view's.
    bool levelSetting(const std::string& key, std::string& value, std::string& error) const;
    bool setLevelSetting(const std::string& key, const std::string& value, std::string& result);
    bool applyLevelSetting(const std::string& key, const std::string& value, std::string& error);
    bool unsetLevelSetting(const std::string& key, std::string& error);
    bool listLevelSettings(const std::string& group, std::string& result);
    bool spawnCameraFromView(std::string& result);
    // The retail host's light presets (App/EditSessionHost.cpp; bf2editor/Content/LightPreset/*.con): "list",
    // "apply" <name> (the level-settings edits its statements name, one undoable edit), "save" <name> (the
    // level's light, fog and sky settings as a preset file), "reset" (those settings back to Sky.con as last
    // opened or saved), "sun" / "flare" <1|-1> (the direction from the camera's view, or its opposite).
    static std::vector<std::string> lightPresetNames();
    // The mod's level folders that hold a Sky.con.
    std::vector<std::string> lightLevelNames();
    bool lightPreset(const std::string& action, const std::string& argument, std::string& result);
    // The material editor (App/EditSessionMaterials.cpp; EditorMaterial.*): "status", "list", "info" <id>,
    // "cells" <id>, "cell" <a> <b>, "set" <id> <property> <value>, "add" <name>, "addAs" <name> <id> <type>,
    // "remove" <id>, "addCell" / "removeCell" <a> <b>, "setDamageMod" <a> <b> <value>, "setTemplate" <a> <b>
    // <effect|decal|sound> <index> <name>, "save", "extract" (the Common archives into the mod's Common folder).
    // Each edit is undoable (applyMaterialEdit is the replay); the files save with the level.
    bool materialCommand(const std::string& action, const std::vector<std::string>& args, std::string& result);
    bool applyMaterialEdit(int id, const std::string& state, std::string& error);
    bool loadMaterials(std::string& error);
    MaterialDocuments& materials() { return m_materials; }
    // Dispatches animation editing and query commands; see docs/editor-console.md for syntax.
    // Edits update the document through undo history; level save writes the files.
    bool animationCommand(const std::string& action, const std::vector<std::string>& args, std::string& result);
    bool applyAnimationEdit(int id, const std::string& state, std::string& error);
    AnimationDocuments& animations() { return m_animations; }
    int animationCurrent() const { return m_animationCurrent; }
    const std::string& animationSelectedKind() const { return m_animationKind; }
    const std::string& animationSelectedName() const { return m_animationName; }
    std::vector<std::string> animationSystemList();
    struct AnimationBar
    {
        std::string name;
        float       start;
        float       length;         // as the engine plays it: the bundle's setAnimationLength, the animation's
        float       original;       // length line, else its .baf's frames / 24; original = the .baf's (-1 unread)
        bool        lengthSet;      // the bundle names a length of its own
    };
    struct AnimationPoint
    {
        std::string name;
        float       time;
    };
    struct AnimationTimeline
    {
        std::string                 bundle;
        float                       span;           // the bundle's length line, else its furthest end
        bool                        lengthSet;
        std::vector<AnimationBar>   bars;
        std::vector<AnimationPoint> points;
    };
    bool animationTimeline(const std::string& bundle, AnimationTimeline& timeline, std::string& error);
    struct AnimationPropertyInfo
    {
        std::string name;
        std::string fallback;       // the engine's value when the file sets none
    };
    // The bound value rows of a kind (a trigger's by its type), the trigger types the engine makes, the system's
    // own rows (the camera spring).
    static std::vector<AnimationPropertyInfo> animationProperties(AnimationKind kind, const std::string& triggerType);
    static std::vector<std::string> animationTriggerTypes();
    static std::vector<std::string> animationSystemRows();
    // A .baf's own length (frames / 24) and joint count from its header; false when unreadable.
    bool bafInfo(const std::string& path, float& seconds, int& joints);
    bool animationsModified() const { return !m_animations.modified().empty(); }
    void planAnimationFiles(SavePlan& plan);
    bool saveAnimationFiles(std::string& result);
    // The mod's mod.desc and the level's Info/<level>.desc: their
    // fields listed ("list", which "mod" or "level"), one set by the user ("set", one undoable edit), and the
    // history's replays ("apply", "remove"). The level save writes them.
    bool description(const std::string& action, const std::string& which, const std::string& field,
                     const std::string& value, std::string& result);
    DescriptionDocuments& descriptions();
    // The retail level editor's lists of the open mod: spawner vehicle presets and teams.
    const LevelEditorDefs& levelEditorLists() const;
    bool materialsModified() const;
    void planMaterialFiles(SavePlan& plan);
    bool saveMaterialFiles(std::string& result);
    bool extractCommonArchives(std::string& result);
    // +looseCommon 1 leaves the Common archives unmounted (the material files then read from the loose copy).
    void setCommonArchivesMounted(bool mounted) { m_commonArchivesMounted = mounted; }
    bool commonArchivesMounted() const { return m_commonArchivesMounted; }
    // Small host and level-editor commands the catalogue left out:
    // "pasteInPlace", "fromSelection" (the camera from the selected object, a spawn point), "snapAll",
    // "saveLists" (Editor/ObjectList.txt and MaterialList.txt), "levels", "open" <level>.
    bool hostTool(const std::string& action, const std::string& argument, std::string& result);
    // Phase E: the terrain (App/EditSessionTerrain.cpp).
    bool terrainInfo(std::string& result);
    bool terrainHeight(float x, float z, std::string& result);
    bool terrainSetBrush(const std::string& key, const std::string& value, std::string& result);
    bool terrainStroke(float x, float z, int ticks, bool secondary, std::string& result);
    bool terrainApplyChange(int id, const std::string& state, std::string& error);
    bool terrainExportHeightmaps(const std::string& directory, std::string& result);
    bool setTerrainTool(bool active, std::string& result);
    bool terrainSectors(std::string& result);
    bool terrainSetSector(int x, int z, bool active, std::string& result);
    bool terrainApplySector(int x, int z, bool active, std::string& error);
    bool terrainImportSurrounding(const std::string& path, std::string& result);
    bool terrainSmoothEdges(int width, float amplitude, float falloff, std::string& result);
    bool terrainSetSeaLevel(float level, std::string& result);
    bool terrainApplySeaLevel(float level, std::string& error);

    // Object bundles and level brushes
    // Both live in the library folder (Framework/ObjectLibrary), in the retail editor's file formats and,
    // by default, its folders, so the two editors share them. Only objects of StaticObjects.con can go
    // into either, since whatever they make goes there.
    ObjectLibrary& library() { return m_library; }
    bool libraryInfo(std::string& result);
    bool setLibraryFolder(const std::string& folder, std::string& result);     // "default": the retail one
    // A bundle is the selection saved: the primary object is its root, and each other object is kept by
    // its offset in the root's frame, its own rotation and its layer. Placing one puts the root at a
    // point and turns the whole bundle by `yaw` degrees about the vertical (retail applied the offsets in
    // world axes, which bent any bundle whose root was turned); it is one undoable edit, the objects
    // become the selection, and with bundleGrouping() they share a new group (the lowest Object.group no
    // object uses), which is how the retail editor kept a bundle together.
    bool listBundles(std::string& result);
    bool bundleInfo(const std::string& name, std::string& result);
    bool saveBundle(const std::string& name, bool overwrite, std::string& result);
    bool placeBundle(const std::string& name, const dice::hfe::Vec3& at, float yaw, std::string& result);
    bool placeBundleHere(const std::string& name, float yaw, std::string& result);
    bool deleteBundle(const std::string& name, std::string& result);
    bool bundleGrouping() const { return m_bundleGrouping; }
    void setBundleGrouping(bool group) { m_bundleGrouping = group; }
    bool setBundleGroup(bool group, std::string& result);
    // The bundle a click in the view will place (ViewportTools), and its turn; "" for none.
    const std::string& armedBundle() const { return m_armedBundle; }
    float armedBundleYaw() const { return m_armedBundleYaw; }
    void armBundle(const std::string& name, float yaw) { m_armedBundle = name; m_armedBundleYaw = yaw; }
    // Groups (Object.group): a new group for the selection, group 0 for it (a placed bundle taken apart),
    // and every object of the primary's group selected. The first two are undoable edits.
    bool groupSelection(std::string& result);
    bool ungroupSelection(std::string& result);
    bool selectGroup(std::string& result);
    int freeGroup() const;          // the lowest group 1..255 no root object has; 0 when all are taken

    // A brush stamps its items around a point: each at the point plus its offset (world axes) plus a
    // random offset, turned by its rotation plus a random turn, and set on the ground when the brush snaps
    // (the object's pivot, until the anchor points are in). One seeded generator draws every number (retail
    // reseeded from the clock for each item). A stroke stamps where the button goes down and again each
    // time the cursor has moved more than the spacing across the ground; an erase stroke removes the
    // objects of the brush's templates within the erase radius. Each stroke is one undoable edit (retail
    // had no undo and no erase). New objects go on layer 1, as a drop does.
    bool listBrushes(std::string& result);
    bool brushInfo(const std::string& name, std::string& result);
    bool selectBrush(const std::string& name, std::string& result);
    const std::string& activeBrush() const { return m_activeBrush; }
    LevelBrushFile* activeBrushFile();
    bool defineBrush(const std::string& name, std::string& result);     // from the selection
    bool addSelectionToBrush(std::string& result);                      // to the active brush
    bool addBrushItem(const std::string& templateName, std::string& result);
    // spacing, snapToGround, randomPositionMin/Max, randomRotationMin/Max, randomScaleMin/Max (x/y/z).
    bool setBrushProperty(const std::string& key, const std::string& value, std::string& result);
    // position, rotation, scale (x/y/z) or template, of item `index` (from 0).
    bool setBrushItem(int index, const std::string& key, const std::string& value, std::string& result);
    bool removeBrushItem(int index, std::string& result);
    bool deleteBrush(const std::string& name, std::string& result);
    bool seedBrush(unsigned seed, std::string& result);
    bool brushToolActive() const { return m_brushTool; }
    bool setBrushTool(bool active, std::string& result);
    void setBrushToolActive(bool active);
    float brushEraseRadius() const { return m_brushEraseRadius; }
    bool setBrushEraseRadius(float radius, std::string& result);
    bool beginBrushStroke(bool erase, std::string& error);
    // One tick at a point on the terrain: a stamp when the stroke has moved far enough, or an erase.
    bool brushStrokeTick(const float at[3], std::string& error);
    void endBrushStroke();
    bool brushStrokeActive() const { return m_brushStroke; }
    bool brushStrokeErases() const { return m_brushStrokeErase; }
    // A script's single stamp and single erase at a point, each one undoable edit.
    bool brushStamp(const dice::hfe::Vec3& at, std::string& result);
    bool brushErase(const dice::hfe::Vec3& at, std::string& result);

    // undergrowth
    // The level's Undergrowth.raw (a material id of Undergrowth.cfg per primary-heightmap sample, 0 for none;
    // Framework/UndergrowthMap) and Undergrowth.cfg, read from the level folder when first used and written
    // with the level (File > Save). The view's undergrowth tool (EditorUndergrowth.tool, U) paints the
    // active material in a disc - the middle button or Shift+left paints 0, erasing - one undoable edit per
    // stroke (EditorUndergrowth.applyChange <id> before|after is the replay). 1.50's undergrowth has no
    // editing hooks (its five are empty), so the renderer keeps the map it loaded: the painted undergrowth
    // grows when the level loads again; the view marks the painted cells meanwhile.
    bool undergrowthInfo(std::string& result);
    bool setUndergrowthTool(bool active, std::string& result);
    bool undergrowthToolActive() const { return m_undergrowthTool; }
    void setUndergrowthToolActive(bool active);
    bool setUndergrowthMaterial(int id, std::string& result);
    int  undergrowthMaterial() const { return m_undergrowthMaterial; }
    bool setUndergrowthRadius(float radius, std::string& result);
    float undergrowthRadius() const { return m_undergrowthRadius; }
    bool beginUndergrowthStroke(bool erase, std::string& error);
    bool undergrowthStrokeTick(float x, float z, std::string& error);
    void endUndergrowthStroke();
    bool undergrowthStrokeActive() const { return m_undergrowthStroke; }
    bool undergrowthStrokeErases() const { return m_undergrowthStrokeErase; }
    // A script's single dab (0 for the value erases), one undoable edit.
    bool undergrowthDab(float x, float z, bool erase, std::string& result);
    bool applyUndergrowthChange(int id, const std::string& state, std::string& error);
    // Loaded on first use; 0 when the level has no undergrowth (or it could not be read: the log says why).
    const UndergrowthMap* undergrowthMap();
    const UndergrowthConfig* undergrowthConfig();
    float undergrowthScale() const { return m_terrain.spacing(); }
    bool undergrowthChanged() const;
    void planUndergrowth(SavePlan& plan);
    // Edits supported Undergrowth.cfg values with undoable text snapshots. Adding materials,
    // types, meshes, or textures requires atlas/data generation that is not implemented.
    bool setUndergrowthSetting(const std::string& key, float value, std::string& result);
    bool setUndergrowthMaterialHeight(int id, float value, std::string& result);
    bool setUndergrowthTypeProperty(int id, const std::string& type, const std::string& key, const std::string& value,
                                    std::string& result);
    bool applyUndergrowthConfig(int id, const std::string& state, std::string& error);
    bool undergrowthConfigChanged() const;

    // Object templates (WP7, the object editor; App/EditSessionTemplates.cpp)
    // A template's lines are edited in place in its files (Framework/TemplateFile, App/TemplateDocuments):
    // a get/set property through the engine's own lines first (ObjectTemplate.activeSafe, then
    // ObjectTemplate.<p> <v>), then its line in the template's .tweak (its .con when it has no .tweak). A
    // line of a property the engine adds to rather than sets (addTemplate, mapMaterial, setPosition ...;
    // `name#index` picks one of several) is changed in the file only, and takes effect when the level
    // loads again. Each edit is one undoable entry (EditorObjectTemplate.applyEdit <id> before|after is
    // the replay). The files are saved with the level (File > Save); a file read from a mounted archive
    // is not edited at all (App/TemplateDocuments).
    TemplateDocuments& templateDocuments() { return m_templateDocuments; }
    bool objectTemplateFiles(const std::string& name, std::string& result);
    bool objectTemplateLines(const std::string& name, std::string& result);
    // What the engine holds for a get/set property of the template (a query).
    bool objectTemplateEngineValue(const std::string& name, const std::string& property, std::string& result);
    bool setObjectTemplateProperty(const std::string& name, const std::string& property, const std::string& value,
                                   std::string& result);
    bool removeObjectTemplateProperty(const std::string& name, const std::string& property, std::string& result);
    bool applyObjectTemplateEdit(int id, const std::string& state, std::string& error);
    void planObjectTemplates(SavePlan& plan);
    bool saveObjectTemplates(std::string& result);
    // A bundle template's children (IBundleTemplate): the engine's list; a child added at a position, one
    // moved or turned, one taken out - each one undoable edit of the engine's list (IBundleTemplate
    // addTemplate / setTransformation / removeTemplate) and of the template's file (the addTemplate line with
    // its setPosition and setRotation lines). A child is named `name` (the first of that name) or `name#n`
    // (the n-th). The level's objects of the template keep their children until they are made again (the
    // level loads again). An undone removal puts the child back at the end of the engine's list; the file
    // keeps its line in place.
    struct TemplateChild
    {
        std::string     name;
        dice::hfe::Vec3 position;
        dice::hfe::Vec3 rotation;           // as ObjectTemplate.setRotation takes it
    };
    bool templateChildren(const std::string& name, std::vector<TemplateChild>& children, std::string& error) const;
    bool objectTemplateChildren(const std::string& name, std::string& result);
    bool addTemplateChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position,
                          std::string& result);
    bool moveTemplateChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position,
                           const dice::hfe::Vec3& rotation, std::string& result);
    bool removeTemplateChild(const std::string& name, const std::string& child, std::string& result);
    // A template's components (the engine's component map; each one named by its class, as
    // ObjectTemplate.createComponent takes it), the component classes this build registers (the
    // ClassManager's dice.hfe.world.TemplateComponent.*), a component added and one taken out - each one
    // undoable edit of the engine (createComponent / deleteComponent; undoing a removal creates the component
    // again and runs its section's lines) and of the template's file (its `rem ---BeginComp:<C> ---` section).
    // A component of a kind the template has already (SingleFireComp and MultiFireComp are both a fire
    // component) replaces that one, in the engine as createComponent does and in the file in its place.
    bool templateComponents(const std::string& name, std::vector<std::string>& components, std::string& error) const;
    // The Object templates panel opened on a template (EditorObjectTemplate.show; the retail object editor's
    // active template): the name waits here until the panel next draws and takes it.
    bool showObjectTemplate(const std::string& name, std::string& result);
    std::string takePanelTemplate();
    // The effect preview (App/EditSessionEffects.cpp; the object editor's effect mode): an object of an effect
    // template made where the view's centre meets the ground, outside the level document; Play, Stop and
    // Pause run the engine's Object.forceStart / forceStop / togglePause on the selected effect objects, or
    // else on the preview.
    // New templates, clones and new children (WP7 step 6; census §6.2, §6.3, §6.6). A new template is a .con
    // of its own and a clone copies its source's files under the new name (Framework/TemplateClone); both are
    // written at once and run through the engine, as retail's Save As wrote at once - neither is an undoable
    // edit. A new child is one undoable edit of its parent's file (its addTemplate line, and a `create` block
    // for the child: an editor-made child lives in its parent's file) and of the engine.
    bool newTemplate(const std::string& className, const std::string& name, const std::string& folder,
                     std::string& result);
    bool cloneTemplate(const std::string& source, const std::string& name, const std::string& folder,
                       std::string& result);
    bool newTemplateChild(const std::string& parent, const std::string& className, const std::string& name,
                          const dice::hfe::Vec3& position, std::string& result);
    // The classes offered for a new template (children false) or a new child: the retail editor's lists
    // (bf2editor/Settings/Init/ResourceInit.con, AddAvailableNewResource / AddAvailableChildResource) when it is
    // installed beside the game, else every template class the engine registers; either way only classes the
    // engine knows.
    static std::vector<std::string> templateClassChoices(bool children);
    // Inherit settings (census §6.9): the source's components the target lacks added, then every set/get
    // property the engine answers for the source (Console.getActiveMethods / isSetGet) that differs on the
    // target set there - through the engine and the target's file, all as one undoable edit. Retail copied
    // only set/get properties too; its row filter (sub_1002DE60) is unread, so the rows that name the template
    // itself or its mesh are left out [CHOSEN].
    bool inheritTemplateSettings(const std::string& target, const std::string& source, std::string& result);
    // Rename (census §6.5): the engine's template takes the new name (setName, then renameTemplate, which in
    // 1.50 adds the new name beside the old one) and every bundle holding it as a child follows
    // (renameChildTemplate); its files' block openings and the addTemplate lines of its parents' files follow
    // in place, and so do the level's records of its objects. Retail updated only the engine and its parent
    // in the view; here the files and the level stay loadable. The file names stay, as retail's did. One
    // undoable edit. A template the level itself defines is renamed with EditorGameplay.rename.
    bool renameObjectTemplate(const std::string& name, const std::string& newName, std::string& result);
    // Change type (census §6.7): a template made in the editor (flag 0x80, retail's own condition) given
    // another class - the class in its block openings rewritten in place, and the engine's template made
    // again from its files with that class (deleteTemplate, then every line of its blocks run in file order),
    // so it holds what the files say. Lines the new class refuses are listed and stay in the file (retail's
    // regenerated file dropped them). Refused while an object of the level uses it. One undoable edit.
    bool changeTemplateType(const std::string& name, const std::string& className, std::string& result);
    // Template edits gathered into one history entry (the wizard's executed items): every template edit made
    // between the two calls replays in order and undoes in reverse; nothing is recorded when none was made.
    // Add another line to an accumulating property (armor effects, barrels, fire rates), the armor-effect, barrel, fire-rate and seat-parent
    // helpers, a child (and its later siblings) turned about the parent, a child's place reset, and a template
    // reverted to its files.
    bool addObjectTemplateLine(const std::string& name, const std::string& property, const std::string& value,
                               std::string& result);
    bool addArmorEffect(const std::string& name, const std::string& effect, std::string& result);
    bool setBarrel(const std::string& parent, const std::string& child, bool on, std::string& result);
    bool setFireRates(const std::string& name, bool single, bool burst, bool full, std::string& result);
    bool setSeatParent(const std::string& vehicle, const std::string& seat, std::string& result);
    bool turnTemplateChildren(const std::string& parent, const std::string& child, float degrees, bool following,
                              std::string& result);
    bool resetTemplateChild(const std::string& parent, const std::string& child, std::string& result);
    bool revertTemplate(const std::string& name, std::string& result);
    // O5, the object editor's viewer aids (App/EditSessionPreview.cpp): a preview object of a template beside the
    // level ("show", "refresh", "reset", "frame", "turntable <deg/s>", "info", "clear"); never saved or picked.
    bool previewCommand(const std::string& action, const std::string& argument, std::string& result);
    unsigned templatePreview() const { return m_preview; }
    void updatePreview();
    // The preview's parts, edited in the view ([added], issue draft 14; App/EditSessionTemplates.cpp): every child of
    // the previewed template's tree where its template places it, and each seat's soldier exit points (the
    // engine's: the seat's setSoldierExitLocation or altSoldierExitPosition in the seat's frame, half a metre up).
    // A part picked in the view (or with EditorPreview.pick) gets the gizmo; letting go writes it as one undoable
    // edit - the child's setPosition / setRotation lines, or the seat's exit line - and the preview is made again.
    struct PreviewPart
    {
        enum Kind { Child, Exit, AltExit };
        Kind            kind = Child;
        std::string     owner;          // the template whose line it is: the child's parent, the exit's seat
        std::string     child;          // the child as the child edits name it ("name#n"); "" for an exit
        std::string     label;          // as drawn: the child's template, or "exit" / "alternative exit"
        std::string     exitRotation;   // an exit's rotation text, kept when it moves
        dice::hfe::Mat4 world;          // where it is
        dice::hfe::Mat4 frame;          // the frame its line is written in (the parent's, or the seat's)
        int             depth = 0;      // 1 for the template's own children
        bool            seat = false;   // a child that is a seat (a PlayerControlObject template)
        bool            camera = false; // a child that is a Camera template
    };
    std::vector<PreviewPart> previewParts();
    bool previewPartsShown() const { return m_previewParts; }
    int pickedPreviewPart() const { return m_pickedPart; }
    void pickPreviewPart(int index) { m_pickedPart = index; }
    // The part `index` moved to `world` (for an exit, only its position counts): the edit, then the preview again.
    bool movePreviewPart(size_t index, const dice::hfe::Mat4& world, std::string& result);
    // O1: the geometry template's settings in the template's .tweak ("list", "set", "remove").
    bool geometryCommand(const std::string& action, const std::string& name, const std::string& spec,
                         const std::string& value, std::string& result);
    void beginTemplateBatch();
    void endTemplateBatch(const std::string& label);
    // The object wizard (App/ObjectWizard; EditorWizard.*): load, rules, run <category>, items, tick
    // <index|label> <0|1>, execute, prompt, answer <text>, cancel, check <row> <0|1>, checklistOk.
    ObjectWizard& wizard() { return m_wizard; }
    bool wizardCommand(const std::string& action, const std::string& argument, std::string& result);
    // Lighting bakes (App/EditSessionLighting.cpp; phase F): the ground hemi map, the editor's minimap picture
    // (the Minimap panel shows it; minimapVersion() counts the captures) and the environment maps, each the
    // retail editor's console line, written at once.
    bool bakeGroundHemi(int resolution, const dice::hfe::Vec3& position, float size, std::string& result);
    bool captureMinimap(int resolution, std::string& result);
    bool generateEnvMaps(std::string& result);
    bool generateLightmaps(int quality, const std::string& parts, const std::string& options, std::string& result);
    // The terrain part of generateLightmaps (the generator already set up): the level's terrain preset, the trace and
    // the copy into the level; and the object part: every static object (or the selected ones) traced from its mesh's
    // samples into a staging folder, the atlas packed, both copied into the level's Lightmaps/Objects.
    bool traceTerrainLightmaps(int quality, bool primary, bool surrounding, bool nearest, bool resume,
                               const std::string& levelDir, const std::string& levelPath, std::string& result);
    bool traceObjectLightmaps(int quality, bool selectedOnly, std::string& result);
    // Roads and splines (App/EditSessionRoads.cpp, App/RoadNetwork.h): opened once the level loaded; the retail rows
    // (`object` SplineManager, Road, RoadTemplate or RoadTemplateTexture) and the editor's own (`object` EditorRoad:
    // docs/editor-console.md), whose edits are undoable; saved with the level when they changed. The batch part
    // (+saveRoadAndClose) compiles every editable road even unchanged; a level without any saves nothing.
    bool openRoads(const std::string& levelName, std::string& report);
    RoadNetwork& roads() { return m_roads; }
    bool roadCommand(const std::string& object, const std::string& method, const std::vector<std::string>& args,
                     std::string& result);
    void planRoads(SavePlan& plan, bool force);
    bool saveRoads(bool force, std::string& result);
    // The view's road tool (EditorRoad.tool): while it is on, a click on the ground adds a point to the active spline
    // (a new spline when there is none), a press on a spline's point makes that spline active and drags the point,
    // Alt+click near the active spline's curve inserts a point there, a right click on a point - or Delete with one
    // selected - removes it. Every one is an undoable EditorRoad edit. Turning it on turns the brushes off.
    bool roadToolActive() const { return m_roadTool; }
    bool setRoadTool(bool active, std::string& result);
    // The spline the road tool and the Roads panel edit, and its selected point (-1 for none). A spline that no longer
    // exists (deleted, undone) counts as none.
    int activeSpline() const { return m_roads.hasSpline(m_activeSpline) ? m_activeSpline : -1; }
    int activeSplinePoint() const { return activeSpline() >= 0 ? m_activeSplinePoint : -1; }
    void setActiveSpline(int id, int point) { m_activeSpline = id; m_activeSplinePoint = point; }
    // Whether the view draws the splines (EditorRoad.showSplines; the retail ToggleSpline).
    bool splinesShown() const { return m_splinesShown; }
    void setSplinesShown(bool shown) { m_splinesShown = shown; }
    // The spline EditorRoad.splineMark marked for a merge and its marked point, -1 for none (the retail spline manager's
    // "select for merge"); the template EditorRoad.connect builds with when none is named.
    int markedSpline() const { return m_roads.hasSpline(m_markedSpline) ? m_markedSpline : -1; }
    int markedSplinePoint() const { return markedSpline() >= 0 ? m_markedSplinePoint : -1; }
    const std::string& activeRoadTemplate() const { return m_activeRoadTemplate; }
    // The level's Roads/*.mesh no editable road compiles and CompiledRoads.con does not load (the retail Clean Road
    // Folder's search), reported; with `move`, moved into Roads/Unused - never deleted.
    bool cleanRoadFolder(bool move, std::string& result);
    unsigned minimapVersion() const { return m_minimapVersion; }
    static bool isEffectTemplate(const std::string& name);
    bool previewEffect(const std::string& templateName, std::string& result);
    bool effectAction(const std::string& action, std::string& result);
    bool clearEffectPreview(std::string& result);
    bool effectInfo(std::string& result);
    static std::vector<std::string> componentClasses();
    bool objectTemplateComponents(const std::string& name, std::string& result);
    // Offers undoable fixes for destroyable templates missing BasicInfo network metadata
    // or an armor component.
    std::vector<std::string> templateFixes(const std::string& name) const;
    bool objectTemplateFixes(const std::string& name, std::string& result);
    bool objectTemplateComponentClasses(std::string& result);
    bool addTemplateComponent(const std::string& name, const std::string& component, std::string& result);
    bool removeTemplateComponent(const std::string& name, const std::string& component, std::string& result);
    // The mod's object archives (Framework/ObjectArchives): whether they are mounted (not in the loose mode,
    // +looseObjects 1), extracting them into the mod's Objects folder (nothing there overwritten; refused
    // without the disk space), and packing the folder into Objects_client.zip / Objects_server.zip in a
    // mod folder - never over the running mod's own archives while they are mounted.
    void setObjectArchivesMounted(bool mounted) { m_objectArchivesMounted = mounted; }
    bool objectArchivesStatus(std::string& result);
    bool extractObjectArchives(std::string& result);
    bool packObjectArchives(const std::string& modFolder, std::string& result);

    // Anchor helpers use engine mesh-space values. Ground snap offsets the pivot by anchor Y,
    // falling back to the bounds bottom; stamping and alignment match world anchors.
    // Template-anchor changes edit shared template state and support undo.
    static bool anchorPosOf(dice::hfe::world::IObject* object, float out[3]);
    static bool worldAnchorOf(unsigned id, float out[3]);
    // Where the object's pivot goes for it to stand on `ground` (the height under its anchor).
    static float snapPivotHeight(dice::hfe::world::IObject* object, float ground);
    // The object's matrix set through the engine's own line, as a replay would; no history entry.
    static bool setTransformNow(unsigned id, const dice::hfe::Mat4& m, std::string& error);
    bool anchorInfo(std::string& result);
    bool setAlignStamp(std::string& result);
    bool alignToStamp(std::string& result);
    bool raiseByAnchor(std::string& result);
    // The primary object's template anchor moved (every object of the template follows): written in place
    // as `ObjectTemplate.anchor x/y/z` in the template's file and set in the engine, one undoable edit
    // (the object editor's, App/EditSessionTemplates.cpp). anchorToBox takes a point of the object's
    // bounding box: bottom (the base's centre), origin, or corner0..corner7; resetAnchor puts back the
    // anchor the file had when it was last saved.
    bool setAnchor(const dice::hfe::Vec3& anchor, std::string& result);
    bool anchorToBox(const std::string& which, std::string& result);
    bool resetAnchor(std::string& result);
    // The move gizmo moves the primary object's anchor instead of the object (EditorGizmo.anchorMode).
    bool anchorMoveMode() const { return m_anchorMoveMode; }
    bool setAnchorMoveMode(bool on, std::string& result);
    unsigned alignStamp() const { return m_alignStamp; }
    // The rotate gizmo's pivot: the primary object's own (0) or its anchor (1).
    bool rotateAroundAnchor() const { return m_rotateAroundAnchor; }
    bool setGizmoPivot(int pivot, std::string& result);

    // Ambient sounds (phase C)
    // AmbientObjects.con's Sound templates. The 1.50 engine makes no object of them
    // (SoundObjectTemplate::createObject returns none), so the editor keeps them itself: the template's
    // `position` line (where the game plays the sound), its `Sound.addTrigger` line, and the record the
    // retail editor kept at the same place in the file's `if v_arg1 == BF2Editor` block. Of the 1,176
    // stock sounds, 1,159 have a position and such a record; the rest play everywhere.
    struct SoundRow
    {
        std::string name;
        bool        positioned;
        float       position[3];
        float       radius;         // minDistance
        std::string file;           // soundFilename
    };
    std::vector<SoundRow> sounds() const;
    bool soundRow(const std::string& name, SoundRow& out) const;
    bool listSounds(std::string& result);
    bool soundInfo(const std::string& name, std::string& result);
    bool moveSound(const std::string& name, const dice::hfe::Vec3& position, std::string& result);
    bool createSound(const std::string& name, const std::string& soundFile, const dice::hfe::Vec3& position,
                     std::string& result);
    bool createSoundHere(const std::string& name, const std::string& soundFile, std::string& result);
    bool deleteSound(const std::string& name, std::string& result);
    // The history's replays: a position, a sound taken out of the file and one put back.
    bool applySoundPosition(const std::string& name, const dice::hfe::Vec3& position, std::string& error);
    bool removeSound(const std::string& name, std::string& error);
    bool restoreSound(const std::string& name, std::string& error);
    bool selectSound(const std::string& name, std::string& result);     // "" deselects
    const std::string& activeSound() const { return m_activeSound; }

    // Combat areas (phase C)
    // The engine's areas (CombatAreaManager) are the running state. An edit replaces the area through
    // the engine's own lines - CombatArea.active + deleteActiveCombatArea, then create and every
    // property, since the engine only appends points - and EditorCombatArea.sync reads the result back
    // into the gameplay layer's file. The history keeps both scripts. The selected area (and point) is
    // the one the view and the Combat areas panel edit.
    static bool engineCombatArea(const std::string& name, LevelObjectFile::CombatAreaData& out);
    static std::vector<LevelObjectFile::CombatAreaData> combatAreas();
    bool listCombatAreas(std::string& result);
    bool combatAreaInfo(const std::string& name, std::string& result);
    // A square of `size` metres around the point.
    bool createCombatArea(const std::string& name, const dice::hfe::Vec3& centre, float size, std::string& result);
    bool createCombatAreaHere(const std::string& name, float size, std::string& result);
    bool deleteCombatArea(const std::string& name, std::string& result);
    // The retail Copy / Paste combat area: an area's
    // points and settings kept, then a new area made from them under another name, one undoable edit.
    bool copyCombatArea(const std::string& name, std::string& result);
    bool pasteCombatArea(const std::string& name, std::string& result);
    bool moveCombatAreaPoint(const std::string& name, int index, float x, float z, std::string& result);
    // Before point `index` (the point count appends).
    bool insertCombatAreaPoint(const std::string& name, int index, float x, float z, std::string& result);
    bool removeCombatAreaPoint(const std::string& name, int index, std::string& result);
    // team, vehicles, layer, usedByPathFinding (0/1), inverted (0/1).
    bool setCombatAreaProperty(const std::string& name, const std::string& property, int value, std::string& result);
    // use (0/1), timeAllowedOutside, damage.
    bool setCombatAreaManager(const std::string& property, float value, std::string& result);
    bool syncCombatArea(const std::string& name, std::string& error);
    bool syncCombatAreaManager(std::string& error);
    bool selectCombatArea(const std::string& name, int point, std::string& result);   // "" deselects
    const std::string& activeCombatArea() const { return m_activeCombatArea; }
    int activeCombatPoint() const { return m_activeCombatPoint; }
    // Whether the view and the minimap draw the areas (EditorView.combatAreas).
    bool combatAreasShown() const { return m_combatAreasShown; }
    bool setCombatAreasShown(bool shown, std::string& error);
    // The manager's master switch and defaults as the engine holds them; false without a manager.
    static bool combatAreaManagerValues(bool& use, float& timeAllowedOutside, float& damage);

    // The marked, pickable root object within `radius` pixels of (x, y), nearest first; 0 for none.
    unsigned markerAt(float x, float y, float radius) const;
    static const float kMarkerReach;        // metres: markers are drawn and picked this near
    void setGridStep(float step) { if (step > 0.0f) m_gridStep = step; }
    size_t clipboardSize() const { return m_clipboard.size() + (m_lastOffsetArmed ? 1 : 0); }
    // E3, the last-offset copy (App/EditSessionLevelTools.cpp): two selected objects of one template arm it;
    // a Paste then adds one more at the same step from the primary (pasteClipboard calls pasteLastOffset).
    bool lastOffsetCopy(std::string& result);
    bool pasteLastOffset(std::string& result);
    bool lastOffsetArmed() const { return m_lastOffsetArmed; }
    // E1, the measure tool: terrain points, the length of the line through them (3D), "Length: %f" at the end.
    bool measureToolActive() const { return m_measureTool; }
    bool setMeasureTool(bool on, std::string& result);
    bool measureAdd(float x, float z, std::string& result);
    bool measureUndo(std::string& result);
    bool measureFinish(std::string& result);
    float measureLength() const;
    const std::vector<dice::hfe::Vec3>& measurePoints() const { return m_measurePoints; }
    // E4: the selected objects (every level object of their templates) or all the level's objects made again
    // from the templates in memory, where they stand, with their properties; not an edit of the level.
    bool reloadObjects(bool selectedOnly, std::string& result);
    // E6: after the terrain's vertical scale changed from the one the level loaded with, each root object's
    // height moved by as much as the ground under it did (one undoable edit).
    bool repositionObjects(std::string& result);
    // L2: the level's Lightmaps/Objects lightmaps (<template>=<n>=<x>=<y>=<z>.dds) no object uses and the atlas pages
    // its LightmapAtlas.tai no longer names, reported; with `move`, moved into Lightmaps/Objects/Unused (never deleted).
    bool cleanLightmapFolder(bool move, std::string& result);
    bool hasCopiedTransform() const { return m_haveCopiedTransform; }

    // An object deleted and made again gets a new engine id; history scripts keep naming it by an
    // older one. resolve() follows those renamings to the id the object has now.
    unsigned resolve(unsigned id) const;
    // Where a new object goes: the point under the middle of the 3D view (terrain or object), or 20 m
    // in front of the camera when the view looks at nothing.
    void setViewCentre(float x, float y) { m_viewCentre[0] = x; m_viewCentre[1] = y; }
    void pointAtViewCentre(float out[3]) const;
    void runDelete();
    void runDuplicate();
    void runCommand(const std::string& line, const char* what);     // a console line, its answer logged
    void runSnapToGround();

private:
    bool saveLevelFiles(std::string& result, bool everything);
    // Every part of a level save, in the order the level save has always written them. False with `error` when no
    // level document is open.
    bool planLevelSave(SavePlan& plan, bool everything, std::string& error);
    // A planned level save written; its answer to `result`.
    bool writeLevelSave(SavePlan& plan, bool everything, std::string& result);
    // A plan written with the level's backup (`label` names what wrote it in the backup); `result` gets the parts'
    // answers. False, with why in `result`, when nothing could be written; a refused part is among the answers, and
    // the caller asks the plan (anyRefused) whether everything was saved.
    bool writeSave(SavePlan& plan, const std::string& label, std::string& result);
    std::unique_ptr<SavePlan> m_pendingSave;
    unsigned                  m_pendingRevision = 0;
    std::string               m_pendingState;           // unsavedSummary() when it was planned
    bool                      m_pendingEverything = false;
    int                       m_backupsKept = 10;
    ISavedDocument*           m_hudDocument = 0;
    bool                      m_previewBeforeSave = false;
    bool                      m_saveRequested = false;
    bool                      m_restored = false;       // a restore wrote the level's files: reopen before saving
    bool makePreview(const std::string& name, const float at[3], float yaw, std::string& result);
    bool clearPreview(std::string& result);
    OverlayOptions             m_overlays;
    bool                       m_overlayCached = false;
    unsigned                   m_overlayRevision = 0;
    size_t                     m_overlayCount = 0;
    unsigned                   m_overlayVisibility = 0;    // m_visibilityRevision when the overlay was made
    unsigned                   m_visibilityRevision = 0;   // bumped when a layer is shown or hidden
    std::vector<OverlayLink>   m_overlayLinks;
    std::vector<OverlayCircle> m_overlayCircles;
    // The engine's own .con lines make the object: Object.create, then its matrix and layer.
    bool makeObject(const std::string& templateName, const dice::hfe::Mat4& transform, int layer, unsigned& id,
                    std::string& error);
    // The record a new or re-made object gets: its template, its matrix's position and angles, its layer.
    static ObjectPlacement placementFor(const std::string& templateName, const dice::hfe::Mat4& transform, int layer);
    // Whether the object may be deleted: it has a record in StaticObjects.con, or in a file that defines
    // its template (which then goes with the template's last object).
    bool deletable(unsigned id, std::string& error) const;
    // A new record of a gameplay class: host-only where the file has no records of the class to follow
    // (spawners, control points), and no zero rotation for a control point (the stock layers write none).
    static void setGameplayFlags(const std::string& className, ObjectPlacement& placement);
    // The area may be edited and saved: the gameplay layer is open, the name is one, the area exists
    // (when `mustExist`; `current` is then filled) and its block is one the editor writes.
    bool combatAreaEditable(const std::string& name, bool mustExist, LevelObjectFile::CombatAreaData& current,
                            std::string& error);
    // One undoable edit that replaces the area `before` with `after` (same name).
    bool replaceCombatArea(const LevelObjectFile::CombatAreaData& before, const LevelObjectFile::CombatAreaData& after,
                           const std::string& label, std::string& result);
    // createGameplayObject and createEffectArea: `effect` only for an effect area.
    bool makeLevelObject(const std::string& kind, const std::string& name, const std::string& effect,
                         const dice::hfe::Vec3& position, std::string& result);
    // Whether a copy of the object can be made and saved: it belongs to StaticObjects.con.
    bool copyable(unsigned id, std::string& error) const;
    // Objects made as one recorded edit (duplicate, paste, replace's new half); they become the selection.
    struct NewObject
    {
        std::string      templateName;
        dice::hfe::Mat4  transform;
        int              layer;
        bool             hasProperties;         // copies carry the original's; new objects the template's
        ObjectProperties properties;
        bool             hasLightSourceMask;    // the record writes the mask (the original's did)
        NewObject() : layer(1), hasProperties(false), hasLightSourceMask(false) {}
    };
    // A copy of the object: template, matrix, layer and properties.
    NewObject copyOf(unsigned id) const;
    bool makeObjects(const std::vector<NewObject>& objects, std::vector<unsigned>& made, std::string& error);
    // The history's scripts for objects the editor made (remade by redo, removed by undo), and the replay
    // of one creation (EditSession.cpp's helpers, for the other files of the session).
    static void madeScripts(const std::vector<unsigned>& made, std::string& doScript, std::string& undoScript);
    static std::string recreateLine(unsigned id, const std::string& templateName, const dice::hfe::Mat4& m, int layer,
                                    const ObjectProperties& properties);
    // The "run /<file>" lines that load these templates, for a bundle or brush file; `keep` are lines a
    // file had already (kept for a template this level does not load).
    static std::vector<std::string> preloadLines(const std::vector<std::string>& templates,
                                                 const std::vector<std::string>& keep);
    bool writeBrush(LevelBrushFile& brush, std::string& error);         // its preload lines renewed
    // One stamp of the active brush at a point; the objects made are added to `made`.
    bool stampBrush(const float at[3], std::vector<unsigned>& made, std::string& error);
    bool eraseBrush(const float at[3], std::string& error);
    void recount();
    // Clears or restores the draw flag so exactly the hidden layers' root objects are hidden.
    void syncVisibility();
    // Objects without a collision mesh, which intersectLine cannot see: the nearest one whose box (or
    // visual bounding sphere) the ray enters, and how far along the ray; 0 when none.
    unsigned pickWithoutCollision(const float origin[3], const float direction[3], float reach, float& distance) const;
    EditSession(const EditSession&);
    EditSession& operator=(const EditSession&);

    // "Move dock_crane #1234" or "Move 3 objects".
    std::string labelFor(const char* verb) const;
    // A terrain change recorded as one history entry (its scripts name it by id); false when it is empty.
    bool recordTerrainChange(const TerrainChange& change, const std::string& label);
    float terrainDistance(const float origin[3], const float direction[3], float reach) const;

    EditorLevel&   m_level;
    FreeCamera&    m_camera;
    LevelDocument* m_document;
    CommandHistory m_history;
    Selection      m_selection;
    GizmoMode      m_mode;
    GizmoSpace     m_space;
    int            m_width;
    int            m_height;

    std::map<unsigned, unsigned> m_remap;       // an id an object once had -> its next id
    float                        m_gridStep;
    dice::hfe::Mat4              m_copiedTransform;
    bool                         m_haveCopiedTransform;
    // The object clipboard: templates, layers, and matrices whose translation is relative to the
    // primary object's when it was copied.
    std::vector<NewObject>       m_clipboard;
    bool                         m_markersShown;
    mutable std::map<unsigned, int> m_markerClasses;    // template class id -> MarkerKind
    float                        m_viewCentre[2];

    std::set<int>         m_hiddenLayers;
    std::set<int>         m_lockedLayers;
    std::set<unsigned>    m_unpickableClasses;
    std::set<unsigned>    m_hiddenByEditor;     // objects whose draw flag the editor cleared
    std::vector<LayerRow> m_layerRows;
    std::vector<ClassRow> m_classRows;
    // Templates the level files define, deleted with their last object: enough to write one again when
    // its object is made again after a save took the block out of the file (lower-case name).
    struct RemovedTemplate
    {
        size_t                                            file;
        std::string                                       className;
        std::vector<std::pair<std::string, std::string> > lines;
    };
    std::map<std::string, RemovedTemplate> m_removedTemplates;
    // Engine templates a rename left behind (lower-case names): no file defines them, and a rename back
    // may use them again.
    std::set<std::string> m_renamedAway;
    // Ambient sounds taken out of the file: enough to put them back after a save (lower-case name).
    struct RemovedSound
    {
        std::vector<std::pair<std::string, std::string> > lines;
        bool            trigger;
        bool            hasRecord;
        ObjectPlacement placement;
    };
    std::map<std::string, RemovedSound> m_removedSounds;
    std::string m_activeSound;          // the selected ambient sound; "" for none
    std::string m_activeCombatArea;     // the selected combat area; "" for none
    int         m_activeCombatPoint;    // its selected point; -1 for none
    bool        m_combatAreasShown;
    bool        m_canRelaunch;
    std::string m_relaunchMode;         // the layer a relaunch opens; "" for none
    std::string m_relaunchLevel;        // the level it opens; "" for the open one
    std::string m_relaunchMod;          // the mod it opens; "" for the open one
    int         m_relaunchSize;
    std::vector<LayerSummary> m_summaries;      // layerSummaries(), built again after an edit or a copy
    unsigned                  m_summariesRevision;
    bool                      m_summariesStale;
    LayerSummary              m_compareLayer;
    bool                      m_compareLayerSet;
    bool                  m_countsStale;
    unsigned              m_seenHistoryRevision;

    // The terrain module (parity phase E). Each recorded stroke keeps its change here, named by the id
    // its history scripts carry (EditorTerrain.applyChange <id> before|after).
    TerrainEditing                m_terrain;
    TerrainBrushSettings          m_terrainBrush;
    bool                          m_paintStroke = false;        // a Paint stroke is under way (no height snapshot)
    std::map<int, TerrainChange>  m_terrainChanges;
    int                           m_nextTerrainChange;
    bool                          m_terrainTool;

    // Object bundles and level brushes (App/EditSessionLibrary.cpp).
    ObjectLibrary                 m_library;
    bool                          m_bundleGrouping;
    std::string                   m_armedBundle;
    float                         m_armedBundleYaw;
    std::string                   m_activeBrush;
    LibraryRandom                 m_random;
    bool                          m_brushTool;
    float                         m_brushEraseRadius;
    bool                          m_brushStroke;        // a stroke under way
    bool                          m_brushStrokeErase;
    bool                          m_haveLastStamp;
    float                         m_lastStamp[2];       // x, z of the stroke's last stamp
    std::vector<unsigned>         m_strokeMade;         // what the stroke made so far
    std::vector<unsigned>         m_strokeRemoved;      // what it erased, and how to make each again
    std::vector<std::string>      m_strokeRecreate;
    int                           m_strokeSkipped;      // items left out (off the map)
    // Anchor points (App/EditSessionAnchors.cpp).
    unsigned                      m_alignStamp;         // the object Align brings anchors onto; 0 for none
    bool                          m_rotateAroundAnchor;
    bool                          m_anchorMoveMode;
    unsigned                      m_effectPreview;      // the effect preview's object; 0 for none
    unsigned                      m_minimapVersion;     // captureMinimap's count, for the Minimap panel
    // The open level's folder ("mods/bf2/Levels/<name>/") and path ("Levels/<name>"), for the bakes.
    bool lightingLevel(std::string& levelDir, std::string& levelPath, std::string& error) const;
    // Object templates (App/EditSessionTemplates.cpp): each recorded edit keeps its file's text before and
    // after, and the engine value before and after when the engine took the line.
    struct TemplateEdit
    {
        size_t      file;
        std::string before;
        std::string after;
        bool        engine;
        std::string templateName;
        std::string className;
        std::string property;
        std::string engineBefore;
        std::string engineAfter;
        int             childOp;            // 0 none, 1 a child added, 2 a child taken out, 3 a child moved,
                                            // 4 a new child template made and added, 5 the template renamed
                                            // (templateName -> child) in the engine and the level, 6 its class
                                            // changed (className -> childClass), made again from its files
        std::vector<size_t>      extraFiles;    // 6: the template's other files, their text before and after
        std::vector<std::string> extraBefore;
        std::vector<std::string> extraAfter;
        std::string              workingFile;   // 6: its defining file, stamped again on the remade template
        std::string     childClass;         // 4: the new template's class
        std::string     child;
        dice::hfe::Mat4 childTransform;     // the child's matrix (3: after the move)
        int             childAt;            // 3: the child's index in the engine's list
        dice::hfe::Mat4 childBefore;        // 3: its matrix before the move
        int                      componentOp;       // 0 none, 1 a component added, 2 one taken out
        std::string              componentAdded;    // 1: the class added
        std::string              componentRemoved;  // 1: the class it replaced ("" for none); 2: the class taken out
        std::vector<std::string> componentLines;    // the removed component's lines, run again on undo
        TemplateEdit() : file(0), engine(false), childOp(0), childAt(-1), componentOp(0) {}
    };
    // One template edit kept and recorded in the history (EditorObjectTemplate.applyEdit <id> after|before),
    // or, while a batch is open, kept for the batch's one history entry.
    void recordTemplateEdit(const TemplateEdit& edit, const std::string& label);
    bool                          m_templateBatching;
    std::vector<int>              m_templateBatch;
    ObjectWizard                  m_wizard;
    std::vector<CameraBookmark>   m_cameraBookmarks;
    // A material edit: both files before and after, the engine lines each way (run after the files are set),
    // and the materials removed directly first (no console row removes one).
    struct MaterialEdit
    {
        std::string      defineBefore, defineAfter, cellsBefore, cellsAfter;
        std::string      engineDo, engineUndo;
        std::vector<int> removeDo, removeUndo;
    };
    bool recordMaterialEdit(const MaterialEdit& edit, const std::string& label, std::string& result);
    void selectExistingMaterialCell();
    // An animation edit: one system file's text before and after (the edits change files only).
    struct AnimationEdit
    {
        size_t      document;
        std::string before, after;
    };
    bool recordAnimationEdit(size_t document, const std::string& before, const std::string& label, std::string& result);
    bool animationEdit(const std::string& action, const std::vector<std::string>& args, std::string& result);
    int  valueHolderDocument(std::string& error);
    struct LayerEdit
    {
        std::vector<GameplayLayer> before, after;
    };
    std::map<int, LayerEdit>      m_layerEdits;
    int                           m_nextLayerEdit = 1;
    GameplayLayerModel            m_layerModel;
    EditableTerrain               m_editableTerrain;
    LightSetup                    m_lightSetups[2];             // L5: object, terrain
    bool                          m_lightSetupModified[2] = { false, false };
    bool                          m_lightSetupLoaded = false;
    int                           m_lightSetupMode = 0;
    bool recordLayerEdit(const std::vector<GameplayLayer>& before, const std::string& label, std::string& result);
    AnimationDocuments            m_animations;
    int                           m_animationCurrent = -1;     // the open system's document, -1 for none
    std::string                   m_animationKind;             // the selected object: its kind ("bundle" ...)
    std::string                   m_animationName;             //   and name; "" for none
    std::map<int, AnimationEdit>  m_animationEdits;
    int                           m_nextAnimationEdit = 1;
    std::map<std::string, std::pair<float, int> > m_bafInfo;   // lowercased path -> seconds, joints (-1 unread)
    MaterialDocuments             m_materials;
    DescriptionDocuments          m_descriptions;
    bool                          m_lastOffsetArmed = false;
    NewObject                     m_lastOffset;
    float                         m_lastOffsetStep[2] = { 0.0f, 0.0f };   // X and Z from one object to the next
    bool                          m_measureTool = false;
    SnapModes                     m_snapModes;
    unsigned                      m_preview = 0;                // O5: the template preview's object
    std::string                   m_previewTemplate;
    float                         m_previewAt[3] = { 0.0f, 0.0f, 0.0f };
    float                         m_previewYaw = 0.0f;
    float                         m_turntable = 0.0f;           // degrees a second
    long long                     m_turntableLast = 0;
    bool                          m_previewParts = false;       // the preview's parts shown and editable in the view
    int                           m_pickedPart = -1;
    void collectPreviewParts(dice::hfe::world::IObjectTemplate* objectTemplate, const dice::hfe::Mat4& world, int depth,
                             std::vector<PreviewPart>& parts);
    void addExitParts(const std::string& seat, const dice::hfe::Mat4& world, int depth, std::vector<PreviewPart>& parts);
    float                         m_verticalScaleAtLoad = 0.0f;     // E6: the terrain's scale.y as the level opened
    std::vector<dice::hfe::Vec3>  m_measurePoints;
    std::map<int, MaterialEdit>   m_materialEdits;
    int                           m_nextMaterialEdit;
    bool                          m_commonArchivesMounted;
    float                         m_clipboardOrigin[3];     // where the copied primary stood (paste in place)
    LevelObjectFile::CombatAreaData m_copiedArea;           // EditorCombatArea.copy
    bool                          m_hasCopiedArea;
    std::string                   m_panelTemplate;      // EditorObjectTemplate.show, for the panel
    // Undergrowth (App/EditSessionUndergrowth.cpp).
    bool loadUndergrowth(std::string& error);
    UndergrowthMap                m_undergrowth;
    UndergrowthConfig             m_undergrowthConfig;
    std::string                   m_undergrowthSaved;       // the map as read or last saved
    bool                          m_undergrowthTried;
    bool                          m_undergrowthLoaded;
    bool                          m_undergrowthTool;
    int                           m_undergrowthMaterial;
    float                         m_undergrowthRadius;
    bool                          m_undergrowthStroke;
    bool                          m_undergrowthStrokeErase;
    std::string                   m_undergrowthStrokeStart;
    std::map<int, UndergrowthChange> m_undergrowthChanges;
    int                           m_nextUndergrowthChange;
    std::string                   m_undergrowthConfigSaved;     // Undergrowth.cfg as read or last saved
    std::map<int, std::pair<std::string, std::string> > m_undergrowthConfigEdits;     // the text before, after
    bool recordUndergrowthConfig(const std::string& before, const std::string& label, std::string& result);
    TemplateDocuments             m_templateDocuments;
    std::map<int, TemplateEdit>   m_templateEdits;
    int                           m_nextTemplateEdit;
    bool                          m_objectArchivesMounted;
    // Roads (App/EditSessionRoads.cpp): an edit is the states of the objects it changed, before and after, applied in
    // order (after) or in reverse order (before) by the history's EditorRoad.applyEdit <id> <before|after>.
    struct RoadChange
    {
        int                kind = 0;            // 0 a spline, 1 a template, 2 a road
        int                splineId = -1;       // a spline's id
        std::string        name;                // a template's or road's name
        bool               existedBefore = false;
        bool               existsAfter = false;
        SplineRecord       splineBefore, splineAfter;
        RoadTemplateRecord templateBefore, templateAfter;
        RoadRecord         roadBefore, roadAfter;
    };
    struct RoadEdit
    {
        std::vector<RoadChange> changes;
    };
    bool roadEditCommand(const std::string& method, const std::vector<std::string>& args, std::string& result);
    bool recordRoadEdit(const RoadEdit& edit, const std::string& label, std::string& result);
    bool applyRoadEdit(int id, const std::string& state, std::string& error);
    bool applyRoadChange(const RoadChange& change, bool after, std::string& error);
    RoadNetwork                   m_roads;
    std::map<int, RoadEdit>       m_roadEdits;
    int                           m_nextRoadEdit = 1;
    bool                          m_roadTool = false;
    int                           m_activeSpline = -1;
    int                           m_activeSplinePoint = -1;
    bool                          m_splinesShown = true;
    int                           m_copiedSpline = -1;            // EditorRoad.splineCopy's spline
    float                         m_splinePasteOffset = 1.0f;     // the next paste's sideways offset in metres
    int                           m_markedSpline = -1;            // EditorRoad.splineMark's spline and point
    int                           m_markedSplinePoint = -1;
    std::string                   m_activeRoadTemplate;           // EditorRoad.activeTemplate
};

} // namespace bf2editor

#endif // BF2EDITOR_APP_EDITSESSION_H
