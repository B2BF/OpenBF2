# Editor console

The editor exposes editing actions through the engine console. The Console panel,
batch scripts, and undo history all use the same command bindings. Start with the
[editor build guide](building.md#editor) for setup and the
[editor architecture](architecture.md#the-editor) for the design.

The editor registers its own families, such as `EditorSelection.*` and
`EditorObject.*`, with `mainConsoleObjects`. The engine's separate `Editor.*`
family belongs to shared `ModuleToolObject` code and remains a recovery frontier;
these editor-only bindings do not implement it.

## Run an edit

With a level open, select a live object, move it one metre upward, and undo the move:

```text
EditorSelection.select 1234
EditorSelection.translate 0/1/0
EditorHistory.undo
```

Replace `1234` with an existing object ID. The console can also resolve object
references written as a template name or `template:n`, where `n` is the zero-based
live root object of that template in ID order. Consult the individual binding
before assuming a command accepts a template reference instead of a numeric ID.

Mutation commands generally return empty text on success. Queries and some edits
return useful output on success, so nonempty output alone is not an error in the
Console panel. Refusals report a reason, including engine parsing errors, unknown
commands, and incorrect argument counts.

For batch operation, `+runConFileAndClose <script>` runs a console file.
Requested `+save...AndClose` operations run after the script. Check the editor's
`+help` output for supported save switches.

## Understand undo

Undo entries contain console scripts. For example, a transform replay activates
an object and sets its full matrix:

```text
EditorObject.activate 1234
Object.absoluteTransformation [1/0/0/0][0/1/0/0][0/0/1/0][10/20/30/1]
```

The undo script stores the old matrix; the redo script stores the new one. Using a
full matrix preserves scale as well as position and rotation. Activation sets
the engine's `Object.*` cursor; failed activation clears it.

History replay uses `runConsoleScript`, which stops on the first editor error or
any nonempty console answer. Replay scripts must therefore use silent setters,
not queries. The interactive runner, `runConsoleLine`, permits successful output.

An action that is neither an editor command nor a property query has an unknown
effect on history. The Console panel and batch runner clear history for such
actions so stale undo entries cannot be replayed. An editor command's presence
in the catalogue does not itself promise undo support: the notes below identify
operations that write immediately or use dedicated replay methods.

## Command catalogue

This catalogue groups commands by workflow. The service interface is declared in
[EditorConsole.h](../Code/Tools/BF2Editor/Framework/EditorConsole.h); the bindings in
[EditorConsole.cpp](../Code/Tools/BF2Editor/Framework/EditorConsole.cpp) are the
current source for exact signatures and additional commands.

These blocks describe syntax, rather than complete scripts. Angle brackets name
arguments; `|` separates alternatives or methods in the same family. After the
first qualified name, a short method name keeps that family unless another
qualified name appears. Parentheses describe behavior. Use quotes for string
arguments containing spaces.

### Objects and selection

Selection commands operate on the open edit session. Pixel coordinates refer to the editor window; vectors use slash-separated world coordinates. Rotations use yaw/pitch/roll in degrees.

```text
EditorObject.activate <id>            the engine's active object ("Object.*" cursor) := object #id

EditorSelection.clear | select <id> | add <id> | pickAt <x> <y> | pickAdd <x> <y> |
                selectTemplate <name> | info

EditorSelection.translate <x/y/z> | rotate <yaw/pitch/roll> | setPosition <x/y/z> |
                setRotation <yaw/pitch/roll>       (undoable; degrees as Object.rotation)

EditorSelection.setLayer <n>                       (undoable: Object.layer)

EditorObject.create <template> <x/y/z> <yaw/pitch/roll> <layer>   (undoable)

EditorObject.placeAt <template> <x> <y>   (undoable: made on the surface under window pixel x, y)

EditorSelection.delete | duplicate | snapToGround                  (undoable)

EditorSelection.box | boxAdd | boxToggle <x0> <y0> <x1> <y1>      (a rectangle of window pixels)

EditorSelection.replace <template> | copyTransform | pasteTransform | snapToGrid <step>

EditorClipboard.copy | cut | paste | pasteAt <x/y/z>

EditorSelection.setProperty <group | disableChildren | notInAI | isOvergrowth | lightSourceMask |
                             controlPointId> <value>

EditorSelection.groupNew | ungroup | selectGroup   (Object.group; the first two undoable)

EditorSelection.anchorInfo | alignStamp | align | raiseByAnchor   (anchor points; the last two
                undoable) | setAnchor <x/y/z> | anchorToBox <bottom|origin|corner0..7> | resetAnchor
                (the template's anchor, undoable) and EditorGizmo.pivot <0 object | 1 anchor> |
                anchorMode <0|1> (the move gizmo moves the anchor)
```

### History and replay

Use the user-facing edit commands to create history entries. Replay methods such as `recreate`, `remove`, `applyChange`, and `applyEdit` are used by stored history scripts.

```text
EditorHistory.undo | redo | clear

EditorObject.recreate <id> <template> <matrix> <layer> | remove <id>   (the history's replays)
```

### Camera, gizmo, and panels

These commands control navigation and the editor shell. The mixed host-command row retains fully qualified names where the command family changes.

```text
EditorCamera.frameSelection

EditorGizmo.mode <0 select | 1 move | 2 rotate> | space <0 world | 1 local>

EditorView.markers <0|1> | combatAreas <0|1>       (the gameplay markers and combat areas)

EditorView.panel <name> <0|1>                      (Log, Engine, Resources, Selection, Objects,
                                                     History, Console, Layers, Profiler)

EditorCamera.mode <0 free | 1 top-down | 2 orbit | 3 follow the terrain> | setPosition <x/y/z> |
             resetPosition | setSpeed <m/s> | resetSpeed | info | addBookmark | toggleBookmark |
             bookmark <1|2> | bookmarks   (the retail host's two camera position bookmarks)

EditorClipboard.pasteInPlace | EditorCamera.fromSelection | EditorLevel.snapAllToGround | saveLists |
             levels | open <level> | EditorView.pauseUpdate <0|1> | levelViewDistance <0|1>   (additional host commands:
             the host's commands the catalogue left out)

EditorTools.customCommands | setCustomCommand <1..3> <line> | setCustomFile <1..3> <file> |
             runCustomCommand <1..3> | runCustomFile <1..3>   (the retail host's custom commands)
```

### Levels and layers

Saving writes level data; packing produces the level archives for a game installation. `saveAs` copies a saved level under a new name and can open the copy. `create` makes a new level beside the open one as the retail editor's Create new terrain did: its files are the retail editor's byte for byte except the differences the row lists. Layer visibility and locking control the editor view.

```text
EditorLayer.hide <n> | show <n> | lock <n> | unlock <n> | select <n>   (the editor's view)

EditorLevel.status | save                          (the level's object files)

EditorLevel.preview | previewAll | previewDiff <n> | writePreview | dropPreview
             (added: the files a save - or with previewAll a resave - would write, numbered, each with what it
             replaces, worked out without writing anything and kept as the pending save; previewDiff shows file n
             against the file on disk, a unified diff for text and the sizes for other files; writePreview writes
             the pending save when nothing changed since the preview, and refuses otherwise)

EditorLevel.backups | restore <name|#n>
             (added: every save is written as a whole, each file it replaces copied first into
             bf2editor/Backups/<mod>/<level>/<date and time>/ with a manifest; a failed write puts back what it
             had replaced, so a save never half-writes the level. backups lists them newest first; restore puts
             the level back to before that save - #0 is the newest - and every save after it, as one more backed-up
             save; it needs every change saved first, and the editor then starts again on the level. Options keep
             the newest 10 by default)

EditorLevel.pack <mod folder>                      (the level's client.zip / server.zip for the game)

EditorLevel.saveAs <name> <open 0|1>               (the saved level copied as Levels/<name>, opened after)

EditorLevel.create <name> <size/scale/height/lowResSize|default> <open 0|1>
             (a new level as Levels/<name>: flat heightmaps at the default height, the retail default textures
             from bf2editor/Content/Terrain on every patch, the level's scripts and terraindata.raw; default is
             the retail dialog's 1024/2/25/256. Built differently: Sky.con keeps the casing Init.con runs it by,
             the map description and the lightmap atlas index carry placeholder text, the patch textures keep
             lowercase names, every patch gets a black low-detail map, the editor maps wait for the first
             terrain save, and the terrain LOD block comes from the open level's terraindata.raw. Added: an
             empty gpm_cq 16 gameplay layer, without which neither the game nor this editor loads the level;
             with open 1 the editor starts again on that layer)

EditorLevel.layers | switchLayer <mode> <size> | duplicateLayer <mode> <size>

EditorGameplayLayers.overview | compare <mode> <size> | compareNone   (every layer's control points and
               combat areas; another layer shown beside the open one)

EditorLevelSettings.list <group|all> | get <key> | set <key> <value> | spawnCameraFromView
               (undoable; apply <key> <value> | unset <key> are the replays)

EditorLayer.status | list | add <name> | rename <id> <name> | filters <id> <"modes"> <"players"> | disable <id>
             <0|1> | active <id> | remove <id> [move|delete]   (the retail layer model: Editor/Layer.con and
             Editor/GamePlayObjects.con, the game-mode files made from them at save; applyLayers the replay)
```

### Gameplay and sound

Gameplay commands create or edit level-owned templates, combat areas, ambient sounds, and supply relationships. The catalogue identifies the lower-level methods used for undo replay.

```text
EditorTemplate.set <template> <property> <value> | apply <..same..> | remove <template> <key> |
               lines <template>                    (templates the level files define)

EditorGameplay.create <kind> <name> <x/y/z> | createHere <kind> <name> |
               createEffectArea <name> <effect> <x/y/z> | createEffectAreaHere <name> <effect>
               (undoable: a spawner, spawn point, control point, trigger or effect area
               with a template of its own) | rename <template> <new name>   (undoable; the template
               and its objects - applyRename <from> <to> is the replay)

EditorCombatArea.list | info <name> | create <name> <x/y/z> <size> | createHere <name> <size> |
               delete <name> | movePoint <name> <index> <x> <z> | insertPoint <name> <index> <x> <z> |
               removePoint <name> <index> | set <name> <team|vehicles|layer|usedByPathFinding|inverted>
               <n> | manager <use|timeAllowedOutside|damage> <value> | select <name> <point> |
               deselect     (undoable edits; sync <name> | syncManager are the replays)

EditorSound.list | info <name> | move <name> <x/y/z> | create <name> <file> <x/y/z> |
               createHere <name> <file> | delete <name> | select <name> | deselect
               (undoable edits; apply <name> <x/y/z> | remove <name> | restore <name> are
               the replays)

EditorGameplay.supplyGroups | groupControlPoints | ungroupControlPoints | connectSupply <team> <supply> <group> |
             disconnectSupply <group>   (supply lines and control-point groups on the selected control points'
             templates; undoable, applyGroup / applySupply the replays)
```

### Object templates, materials, and animation

These tools edit their corresponding template, material, animation, or wizard data. Use each domain's save command where listed; individual operations identify their replay commands.

```text
EditorMaterial.status | list | info <id> | cells <id> | cell <a> <b> | set <id> <property> <value> |
             add <name> | addAs <name> <id> <type> | remove <id> | addCell <a> <b> | removeCell <a> <b> |
             setDamageMod <a> <b> <value> | setTemplate <a> <b> <effect|decal|sound> <index> <name|""> |
             save | extract   (the material editor: Common/Material's two files in place and the engine
             live; undoable, applyEdit <id> <before|after> the replay)

EditorAnimation.systems | open <path> | status | tree | list <kind> | select <kind> <name> | selected |
             set <property> <value> | unset <property> | setSystem <property> <value> | unsetSystem <property> |
             addTrigger <type> <name> | addBundle <name> | addValueHolder <name> <"min normal max"> |
             addAnimation <.baf> | addEvent <name> | removeAnimation <.baf> | removeEvent <name> |
             setStart <animation|event> <name> <seconds> | setLength <.baf> <seconds> | link <child|bundle>
             <name> | unlink <child|bundle> <name> | delete | timeline [bundle] | joints <folder> [list] |
             copySystems <template> <source template> | point <template> <1p|3p|seat> <path> | save
             (the animation editor: the .inc files in place, on the selected object; undoable, applyEdit
             <id> <before|after> the replay)

EditorObjectTemplate.files <template> | lines <template> | engineValue <template> <property> |
                set <template> <property[#index]> <value> |
                remove <template> <property[#index]> | children <template> | addChild <template> <child>
                <x/y/z> | moveChild <template> <child[#n]> <x/y/z> <yaw/pitch/roll> | removeChild
                <template> <child[#n]> | components <template> | componentClasses | addComponent
                <template> <class> | removeComponent <template> <class> | show <template> | new <class>
                <name> [folder] | clone <template> <name> [folder] | newChild <parent> <class> <name>
                <x/y/z> | inherit <template> <source> | rename <template> <new name> | changeType
                <template> <class> | fixes <template> (a destroyable object's missing networkable info
                or Armor, as the retail object editor offered them) | save
                (the object editor: in place in the
                template's .tweak / .con; set and remove are undoable, applyEdit <id> <before|after> the replay)

EditorPreview.show <template> | refresh | reset | frame | turntable <deg/s> | camera <n> | info | clear
                (the object editor's viewer: an object of the template beside the level, never saved or picked,
                with a grid, its bounds and its centre)

EditorPreview.showParts <0|1> | parts | pick <n|-1> | movePart <n> <x/y/z> | turnPart <n> <degrees>
                (added: the preview's parts in the view - every child where its template places it, seats and
                cameras marked, and each seat's soldier exit points, setSoldierExitLocation and
                altSoldierExitPosition in the seat's frame and half a metre up, as the engine puts a soldier.
                A click on a part's mark picks it and the gizmo moves it, or turns a child in the rotate mode;
                letting go writes the child's setPosition / setRotation or the seat's exit line, one undoable
                edit, and the preview is made again. movePart and turnPart do the same by a world offset or a
                turn about the world's up)

EditorWizard.load | rules | run <category> | items | tick <index|"label"> <0|1> | execute | prompt |
                answer <text> | cancel | check <row> <0|1> | checklistOk  (the object wizard: the retail rule
                files over the selected object's tree; resolutions edit the template files in place)
```

### Terrain and vegetation

Terrain strokes and vegetation painting have dedicated replay records. The commands below distinguish world coordinates from sector indices and serialized configuration.

```text
EditorTerrain.info | height <x> <z> | brush <mode|size|strength|raise|lower|height> <value> |
              stroke <x> <z> <ticks> | strokeSecondary <x> <z> <ticks> | tool <0|1> |
              exportHeightmaps <directory> | sectors | sector <x> <z> <0|1> |
              importSurrounding <raw file> | smoothEdges <width> <amplitude> <falloff> | seaLevel <h>
              (applyChange <id> <before|after>, applySector <x> <z> <0|1> and
              applySeaLevel <h> are the replays)

EditorTerrain.view | technique <n>   (the editable terrain's look: 0 colormap and lightmap, 1 grid,
              2 topographic grid, 3 detail layers, 4 undergrowth overlay, 5 overgrowth materials,
              6 overgrowth shadow, 7 base pass; 5 and 6 need a level with overgrowth)

EditorTerrain.brush mode overgrowth | brush overgrowth <material|shadow> | brush material <1..15> |
              brush shadow <0..255>   (the overgrowth brush: a stroke paints the level's overgrowth
              material or shadow map, the other button erases. Not undoable) | generateOvergrowth
              (regrows the plants from the maps in the editor's staging folder; the level's overgrowth
              files are written when the level is saved: the maps and the regenerated collision
              script, and the atlas only when the overgrowth's types changed. A level opens with its
              overgrowth in editor mode, as the retail editor opens it: static objects marked
              isOvergrowth, such as Karkand's palms, draw as themselves, the plants are grown from the
              maps, and the objects the game makes from OvergrowthCollision.con are not in the level)

EditorTerrain.showSector <x> <z> <0|1> | shownSectors   (the surrounding sectors drawn around the
              primary tile, x and z in -1..1; the sectors marked modified - the ones the terrain save
              compiles into the game's ring - are shown when the level opens. The view only: whether a
              sector is saved is EditorTerrain.sector)

EditorTerrain.compile   (writes the painted maps into the staging folder, then compiles the terrain
              there the way the retail editor's terrain save does: Levels/<level>/terraindata.raw - per
              patch the vertices, bounds and detail layers (from the editor detail maps), the surrounding
              sectors marked modified (Heightdata.con's setModified or EditorTerrain.sector, as the retail
              save chose them: a new level has none), and the culler; the level's own file supplies the
              LOD block and the header's other values - and the game textures of the patches painted since
              the last compile: their detail maps (the editor detail maps halved, R5G6B5 when the patch
              draws a layer above 2, else DXT1), their low-detail maps (red the layers of low-detail type 1,
              blue those of type 2, as the renderer builds them; halved to 256, DXT1 with mips) and
              their colormaps (DXT1 with mips). Every patch's low-detail map is rebuilt when a layer's
              low-detail type changed; none while the types are unknown (see EditorGround). The level's
              files are not changed; EditorLevel.save compiles and copies them in whenever the heights,
              the paint or the ground properties changed, with the heightmaps when the heights did.
              Built differently: only painted patches get new textures (the retail save rewrote them
              all, and the surrounding sectors' low-detail maps, from editor maps a stock level does
              not have); the textures come from the editor's own encoders, not nvdxt; and a painted
              patch's colormap is its editor colormap - the retail editor rendered the patch with its
              roads into it. Saving changed heights needs +editableTerrain 1)

EditorTerrain.brush mode paint | brush paints <detail|color|both> | brush layer <0..5> |
              brush colorSource <texture|color> | brush color <r/g/b>   (the paint brush; a stroke
              paints the renderer's edit maps) | merge <detail|color|both|-> |
              clearEdits <detail|color|both|->   (keep or drop the strokes made since the last merge;
              - takes the brush's own setting. The level's save writes the painted maps into its
              Editor folder and compiles the terrain, so the game draws the paint: see
              EditorTerrain.compile. A save writes unmerged detail strokes too, and clearing them
              afterwards marks their patches changed, so the next save writes them without the
              strokes; the renderer's save commits colour strokes into the colormap, where clearEdits
              no longer reaches them)

EditorGround.layers | set <layer> <property> <value> | misc <property> <value> | removeLayer <layer>
              (Editor/GroundProperties.con, saved with the level; undoable, apply <layer> <property>
              <value> is the replay. Layer properties: detailTexture, colorTexture, color <r/g/b>,
              material, topTiling, sideTiling <x/y>, yOffset, planeMap <0|1>, colorTiling <x/y>,
              lowDetailType, envMap <0|1>; misc, or layer -1: farTopTilingHi, farTopTilingLow,
              farSideTiling <x/y>, farYOffset, lodSwitchDistance. The material and the far settings
              are written to the file but not yet applied to the renderer. Painting colour from a
              layer's texture, and erasing colour, need the layer's colour texture. lowDetailType: 0
              none, 1 red, 2 blue of the low-detail map. Added: when no layer has a low-detail type
              (the compiled terrain holds none, and no stock level ships the file), the open recovers
              the types from the level's compiled detail and low-detail maps; when they do not explain
              the maps closely the types stay unknown and the save keeps the level's low-detail maps)

EditorUndergrowth.info | tool <0|1> | material <id> | radius <m> | paint <x> <z> | erase <x> <z>
              (the density map, Undergrowth.raw; applyChange <id> <before|after> is the replay) |
              setting <key> <value> | materialHeight <id> <value> | type <id> <type> <key> <value>
              (Undergrowth.cfg; applyConfig <id> <before|after> is the replay)
```

Both of the following deliberately differ from the retail editor, which loses a stock level's
layers on a terrain save. When a level has no `Editor/GroundProperties.con`, the ground
properties start from the compiled terrain's layers in `terraindata.raw`. Retail started with
none, and its terrain save then wrote a `terraindata.raw` without layer names. When a level has
no editor detail maps, the editor converts the level's compiled detail maps into its staging
folder. Retail started from default maps that put one layer everywhere. Saving painted maps keeps
the heightmap's materials (sounds, effects and grip) as the level's own file has them; retail's
save rebuilt them from the layers' materials, which zeroed a stock level's. The level's own
files change only when it is saved.

### Roads and splines

The level's editable roads open with it: the mod's road templates (the `if v_arg1 == BF2Editor`
branch of every `Objects/Roads/Splines/<name>.con`, in the object archives or a mod's loose objects
folder), then the level's `Editor/Splines.con` and `Editor/Roads.con`. A road is the renderer's
editable road built on a spline with a template and drawn by its road queue; it opens as a ghost,
the quick preview the retail editor showed first. Edits are undoable; `applyEdit <id> <before|after>`
is the replay.

```text
EditorRoad.info | splines | roads | templates | spline <id> | road <name> | template <name>
              (queries; template answers the script the save would write)

EditorRoad.splineCreate | splineCreateAs <id>   (an empty spline; new splines snap added and moved
              points to the ground - snap modes 3) | splineAdd <id> <x/y/z> |
              splineInsert <id> <index> <x/y/z> | splineMove <id> <index> <x/y/z> |
              splineRemovePoint <id> <index> | splineSet <id> <snapModes|lineWidth|stepSize> <value> |
              splinePoint <id> <index> <speed|delay|direction|sound> <value> |
              splineDelete <id>   (with the road on it)
              (snap bits: 1 added points, 2 moved points, 4 curve samples, 8 ribbon edges. A road
              runs between the second and the second-to-last point and needs four points to be built)

EditorRoad.templateCreate <name> | templateCreateFrom <name> <template> |
              templateSet <name> <width|maxTesselatedError|blendFactor|priority|material|
              fadeOffDistance|texture0|texture1|mirror0|mirror1|scale0|scale1> <value> |
              templateDelete <name>   (texture 0 is the primary layer)

EditorRoad.create <name> <spline id> <template> [<noise seed>] |
              set <road> <textureScaleOffset|persistence|range|octaves|priority|fadeStartLeft|
              fadeStartRight|fadeEndLeft|fadeEndRight|seed|spline|template> <value> |
              rename <road> <name> | delete <road>
              (a new road's edge noise is the retail road handler's: persistence 0.2, range
              -0.15..0.15, three octaves)

EditorRoad.build <road|*> <final|ghost>   (the drawn road; not saved: a road opens as a ghost)

EditorRoad.apply <road>   (the retail Apply spline: the ground under the road's footprint - its template's
              width plus 2 m and the noise's range - set to the road's surface through the editable heightmap
              cluster, then the road rebuilt; one undoable terrain change. The editor's renderer takes the 8-bit
              target, as the 2006 editor did: the 1.50 body takes a float target on current cards and writes no
              height there. It sizes the projection's target to the road (not the card's largest texture, whose
              copy back failed for memory in the 32-bit editor and flattened the whole rectangle to the road's
              lowest point) and tries a smaller one when the copy fails. A projection that still comes back as one
              level while the road rises is refused and undone)

EditorRoad.splineCopy [<id>] | splinePaste   (the retail copy and paste: a paste is the renderer's offset copy
              of the copied spline - settings and point properties included - 1 m to its side, 1 m further
              each paste, made a new spline and the active one; undoable)
EditorRoad.splineMark [<id> <point>] | splineMerge [<id> <point>]   (the retail merge: a spline marked at
              the point it joins at, then merged with the given - or active - spline at its point: when that
              point is not its first, the marked spline's points follow its last, in order from a marked
              first point and reversed otherwise, and the marked spline goes; when it is its first, its points
              follow the marked spline's last and it goes. One undoable edit. A spline carrying a road is not
              merged away: disconnect it first)

EditorRoad.activeTemplate <template> | connect <name> [<template>] | disconnect   (the retail Connect and
              Disconnect: a named road on the active spline with the active - or given - template, refused
              "No active spline!" or "No active RoadTemplate!"; the active spline's road removed, refused
              "Spline has no road!". Both undoable)
EditorRoad.refresh   (every road rebuilt from its template, as the retail RefreshRoads did)
EditorRoad.cleanFolder [0|1]   (the retail Clean Road Folder's search: the level's Roads/*.mesh no editable
              road compiles and CompiledRoads.con does not load, reported - retail opened them for delete,
              which on a stock level was every road - and with 1 moved into Roads/Unused; nothing is deleted)

EditorRoad.tool <0|1> | activeSpline <id|-1> [<point>] | showSplines <0|1>
              (the view's road tool: a click on the ground adds a point to the active spline - a new spline
              without one -, a press on a point makes its spline active and drags the point, Ctrl+click or
              Delete removes a point, Alt+click near the active spline's curve inserts one; every one an
              undoable edit. The splines are drawn as their curves and points; the Roads panel lists the
              splines, roads and templates with their settings)

EditorRoad.save   (the roads alone, every editable road compiled even unchanged; EditorLevel.save
              writes them when they changed, as does +saveRoadAndClose and +saveAndClose)

SplineManager.CreateInstance | SetId | SetControlPointSnapModes | setLineWidth | setStepSize |
              AddControlPoint <x/y/z> | SetSpeed | SetDelay | SetDirection | SetSound | End
Road.CreateInstance "<name>" <spline id> "<template>" [<seed>] | SetTextureScaleOffset |
              SetPersistance | SetRange | SetOctaves | Prio | FadeDistAtStartLeft | FadeDistAtStartRight |
              FadeDistAtEndLeft | FadeDistAtEndRight
RoadTemplate.CreateInstance | SetName | SetWidth | SetMaxTesselatedError | SetBlendFactor |
              SetFadeOffDistance | SetPrio | SetMaterial | SetIsPrimaryTexture <1|0>
RoadTemplateTexture.SetTextureFile | SetMirror | SetScale
              (the retail editor's rows the three files are written in; each builds the spline, road
              or template last begun. Not undoable. The retail UI rows - SelectSpline, the Undo rows
              and the like - are refused)
```

A save writes `Editor/Splines.con` and `Editor/Roads.con` as the retail writers did, compiles each
road through the renderer (its `CompiledRoads.con` entry and `Roads/<road>_compiled.mesh`), and writes
each changed template's script with its `_compiled.con` and `_compiled.dat` into the mod's loose
objects folder. Built differently from retail: `CompiledRoads.con` keeps the level's other compiled
roads (retail rewrote it from its editable roads only, which emptied every stock level's file: no stock
level ships editable sources); a template is written only when it changed; a level that never had
editable roads gets no road files; each compiled mesh gets the chunk table the 1.50 compile leaves out
(contiguous triangle runs of up to 1024 triangles and a 32 m half-diagonal, each with its bounding
sphere); a spline ended without `SetId` takes the next free id. Added: a template's script carries
`RoadTemplate.SetFadeOffDistance` when its fade-off differs from the default (retail never wrote it,
so the value reverted on the template's next compile); applying a road to the terrain is undoable.

### Play testing

Soldier mode walks the open level as the local player. The soldier, its kit and any vehicle it
drives are never part of the level. The client's game logic runs, so nothing takes damage or
respawns.

```text
EditorPlayTest.soldier | leave | info | kit <team> <kit>   (a soldier of the level's kit where the
              camera is; leave returns to the editor camera at the soldier's eyes)

EditorPlayTest.drive | driveTemplate <template>   (the selected object spawner's vehicle, or a vehicle
              made in front of the soldier, with the soldier as the driver) |
              hold <input> <value> <steps>   (holds a player input for a number of steps; for scripts)

EditorPlayTest.weapon <template>   (the retail object editor's weapon test: a soldier with no kit where the
              camera is, given the GenericFireArm template and holding it - fire with the left button, R
              reloads; soldier mode starts again when it is on. The fire and ammo components read their
              template as they go, so a rate of fire or a reload time set with EditorObjectTemplate.set reaches
              the held weapon at once; a new magazine size takes another test. info shows the held weapon's
              rounds, magazines and reload)
```

The Object templates panel shows Test this weapon, with the rate of fire, reload time, magazine size and
number of magazines beside it, for a weapon template. Stock weapons are read-only while the object archives
are mounted; tune a clone, or the mod's own loose weapons.

### The HUD and the HUD editor

Added; the retail editor left the HUD scripts to a separate tool. The editor loads the game's HUD
(`Menu/HUD/HudSetup/HudSetupMain.con`) unless it is started with `+hud 0`. The renderer draws it in soldier
mode exactly as the game does, and nowhere else unless the preview is on. View > HUD editor opens the panel. Edits change the HUD scripts
through the undo history, apply to the live HUD where that is safe, and are saved with the level.

```text
EditorHud.info | preview <0|1>   (the HUD drawn without soldier mode, in its in-game mode) |
              set <variable> <value>   (a HUD variable held at a value while previewing; "-" lets it go)

EditorHud.select <part> | node <part>   (the part the panel and the view show - the rows that show it are
              held true while previewing - and its kind, script line, rectangle, rows and show conditions)

EditorHud.rect <part> <x/y/width/height> | setRow <part> <n> <words> | addRow <part> <method> <words> |
              removeRow <part> <n>   (one undoable edit each: the create row's rectangle, row n's words, a new row
              after the part's own rows, a row taken out) | changes   (the scripts edits changed)

EditorHud.setLine | insertLine <script> <line> <text> | removeLine <script> <line>   (the history's own rows:
              the text percent-encoded, so its blanks and quotes survive the console)

EditorTest.mouseToHud <part> <0 centre | 1 size handle>   (the mouse to the part's outline in the view)
```

Rectangles count on the HUD's 800x600 canvas from the parent's corner. Position, size, colour, fade
times, textures, rotation and text apply to the live HUD at once; other rows (show variables, effects,
fonts) and removed rows take effect the next time the editor starts. The two sliding areas
(`BottomLeftAnimate`, `BottomRightAnimate`, which hold the soldier's health and ammo bars) take their
place from the game's own HUD code, which needs a player. Edit their parts in soldier mode: press Esc to
free the mouse, and the outline follows the part where the game puts it.

The level save writes each changed script into every archive mounted on its folder that holds it. The
game client reads `Menu_client.zip`; the editor mounts `Menu_server.zip` before it. Each archive is
written again with its other entries copied exactly as stored. The engine unmounts those archives while
the save replaces them and mounts them again, in their order, afterwards. Only archives inside the open
level's mod are written; a HUD that comes from another mod's archive is refused, with the reason.

### Libraries, bundles, and brushes

Bundles and brushes live in the configured editor library. Brush definition edits write their file immediately; placing bundles, stamping, and erasing create undoable edits.

```text
EditorLibrary.info | folder <path | default>       (where bundles and brushes are kept)

EditorBundle.list | info <name> | save <name> | saveOver <name> | place <name> <x/y/z> <yaw> |
             placeHere <name> <yaw> | group <0|1> | delete <name>      (placing is undoable)

EditorBrush.list | info <name> | select <name> | define <name> | addSelection | addItem <template> |
            set <key> <value> | item <index> <key> <value> | removeItem <index> | delete <name> |
            seed <n> | tool <0|1> | eraseRadius <m> | stamp <x/y/z> | erase <x/y/z>
            (brush edits write the brush's file at once; each stamp or erase is undoable)
```

### Lighting and effects

Light presets apply through undoable level settings. Lighting generation writes its output immediately. Effect previews are temporary objects and are never saved with the level.

```text
EditorLight.presets | apply <preset> | save <name> | resetToLevel | sunFromCamera <1|-1> |
             flareFromCamera <1|-1>   (the retail host's light presets, applied as undoable level
             settings; the sun or flare direction from the camera's view)

EditorLighting.groundHemi <resolution> <x/y/z> <size> | minimap <resolution> | envMaps  (the
                ground hemi map, the editor's minimap picture, the environment maps; written at once)

EditorLighting.lightmaps <quality 0..3> <parts> <-|nearest|resume|nearest+resume>
                (lightmaps as the retail Compile > Generate Lightmaps traced them, with the renderer's
                lightmap generator at resolution 0.25, 0.5, 1 or 2. <parts> joins with +: primary,
                secondary or both - the terrain's, lit by the level's Editor/TerrainLight.con run with
                BF2Editor (the retail editor's terrain preset) or, for a level without one, the preset
                the retail editor writes for a new level; the primary patches into Lightmaps/, the ring
                patches into Editor/Lightmaps/; needs +editableTerrain 1 - and objects or selected - the
                static objects', every one or the selected ones, traced from each mesh's
                Meshes/<mesh>.samples (the editor mounts bf2editor/LightmapSamples*.zip at Objects, as
                the retail editor did; no game archive holds them), lit by Editor/ObjectLight.con or the
                same new-level preset, High tracing objects at 1 as retail did. The level's atlas is cut
                back into single lightmaps first, its own single lightmaps from Editor/Lightmaps/Objects
                over them, so the objects a trace does not reach keep theirs; the atlas the game reads is
                then packed again (StaticMeshRenderer.loadAllLightMapFilesDLL: LightmapAtlas.tai and its
                pages, into Lightmaps/Objects/) and the traced single lightmaps go into
                Editor/Lightmaps/Objects/. Pages of the old atlas the new index no longer names are
                reported. Resume keeps Logs/LightmapGeneration.log. Needs the editor's renderer, which
                binds the 2006 editor's lightmapGeneration object and terrain.drawPrimaryLightmaps /
                drawSurroundingLightmaps. Written at once, not undoable)

EditorLighting.cleanLightmaps <0|1>  (the object lightmaps in Lightmaps/Objects no object uses, matched by
                template and truncated position, and the atlas pages LightmapAtlas.tai no longer names;
                reported, or with 1 moved into Lightmaps/Objects/Unused - nothing is deleted)

EditorEffect.preview <template> | play | stop | pause | clear | info  (the object editor's effect mode:
                a preview made at the view's centre, never saved; play/stop/pause act on the selected
                effect objects, else on the preview)
```

### Mod and object archives

These commands manage the mod's loose objects and archives. Opening another mod restarts the editor on that mod.

```text
EditorObjects.status | extract | pack <mod folder>  (the mod's object archives and its loose Objects folder)

EditorMod.list | create <name> | open <name>     (the mod manager; open starts the editor again on the mod)
```

## Add or maintain a binding

`IEditorCommands` is the open level's service interface, implemented by the edit
session. `registerEditorConsole` borrows that implementation and registers the
tagged bindings with the engine registry. Keep the implementation alive until
`unregisterEditorConsole`; unregister removes the bindings and clears the session
pointer. Test input, play testing, and shell panel callbacks have separate owners.

Keep command parsing and console registration in `EditorConsole.cpp`, and put
editing behavior in the owning session or module. Report refusals through the
supplied result/error text. Preserve the distinction between an ordinary command
that returns a label or query result and a silent replay command.

For an undoable operation, record scripts that restore the exact before and after
state. Validate both replay directions and failure reporting. Commands that
write files immediately should say so in their documentation and UI. Editor
output must remain loadable by the retail game, as described in the editor
architecture.

This interface is editor-only and engine-dependent. It is not part of the
engine-independent `bf2_editor_checks` target.
