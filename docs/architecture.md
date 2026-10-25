# Engine architecture

## Images and boundaries

The repository contains several images built from overlapping source sets. The root
`CMakeLists.txt` defines the engine and client manifests; the directory tree alone does
not determine what links into an image.

`DEDICATED=1` selects the dedicated engine. `DEDICATED=0` selects the client and client
DLL code. Every engine compilation force-includes `Core/BuildConfig.h`, which rejects a
missing image define. A runtime `isDedicated()` check still matters in the client image:
the retail client supports a dedicated mode, so a runtime branch cannot automatically be
replaced by conditional compilation.

The client uses separate renderer (`RendDX9`), sound (`BF2Audio`), movie (`SwiffPlayer`),
and Python (`dice_py`) components. Rebuilt renderer, sound, and movie plugins target the
rebuilt host's native ABI and share its dynamic CRT and release STL layout. Mixing these
plugins with the retail executable, or retail engine plugins with the rebuilt executable,
is unsupported. The AI side-DLL has its own manifest and build target.

This is an intentional policy change under [issue #7](https://github.com/B2BF/OpenBF2/issues/7),
`[CHOSEN, not recovered]`. Gameplay compatibility with retail clients and existing content
remains a requirement. The first removal pass retires compatibility plugin builds and
the `Memory.dll`/MSVCR71 heap adapters, and stages a rebuilt TextureAtlasBuilder with the
client. TextureAtlasBuilder retains its static CRT and legacy float formatting because
those choices preserve `.tai` output, independently of executable compatibility.

The cleanup is staged. Renderer-internal `vc71::*` containers, fixed-offset consumers,
factory calling-convention thunks, and legacy declarations used by shared code and reference
fixtures remain until their callers can be migrated together. `BF2_PLUGIN_ABI_NATIVE=0`
is rejected for renderer, movie, and audio plugin images; its remaining uses do not provide
a supported retail plugin build. Dedicated/non-x86 alternatives and unresolved native
frontiers retain their existing behavior.

SwiffPlayer's wrapper now contains only its native host paths; the retail-host conditional
arms have been removed under the same issue. The native ActionScript callback whitelist and
the `swiff-native-as-host` and `swiff-host-bodies` frontiers remain, along with the RW-88
callback/tracing, RW-89 crash-tracing, and RW-49 module-consolidation boundaries. The documented
`voipStopTest` report-and-continue exception is preserved. Its internal `vc71::*` strings, player-stat
records, and queued-mouse storage still have live consumers and remain for a later migration.
This removal does not change movie formats, callback behavior, or host interface layouts.

## Source map

All engine paths below are relative to [Code/BF2](../Code/BF2/).

| Area | Responsibilities and starting points |
| --- | --- |
| `Game/Main/` | Process startup, `BF2`, `BF2Engine`, module assembly, and the main loop. |
| `Core/` | Class and singleton registration, interfaces, events, settings, platform services, and diagnostics. |
| `IO/Console/`, `IO/File/` | Console commands and scripts, filesystem access, archive mounting, and resource lookup. |
| `IO/Network/` | Sockets, packets, connection transport, and networkable descriptions. `ProtocolLayer/` holds `NetServer` and `SWConnection`. |
| `Game/GameServer/`, `Game/GameClient/` | Session ownership, connection handling, loading, and server/client update paths. |
| `Game/GameLogic/`, `Game/Python/` | Level loading, game rules, and the embedded Python interface. |
| `Game/Objects/`, `Scene/` | Object templates, live world objects, components, and world registration. |
| `Game/GameEvents/`, `Game/Common/` | Serialized game events, shared game state, and ghost replication. |
| `Physics/`, `Math/`, `Animation/` | Simulation, collision, transforms, skeletons, and animation. |
| `Geom/`, `RendDX9/`, `RendMisc/` | Geometry resources, terrain, rendering, and renderer support. |
| `Menu/`, `Sound/` | Client interface and audio integration. |
| `External/` | Upstream dependencies and their local integration notes. |

[Code/Tools/BF2Editor](../Code/Tools/BF2Editor/) contains the editor application.
[Code/Tests](../Code/Tests/) contains focused fixtures and compile checks;
[Code/Build](../Code/Build/) contains build helpers and diagnostic harnesses.
These harnesses are scaffolding, not alternate production engine implementations.

## From startup to a running server

The production entry in `Game/Main/BF2.cpp` creates `BF2`, calls `init`, runs `BF2::run`
until it returns false, then destroys the application. `BF2Engine` owns engine startup,
command-line settings, the main loop, and shutdown. `Core/Module` and `ClassManager`
provide registered factories and named singletons used during assembly.

For a dedicated session, `BF2::init` reaches `BF2Engine::startGame`, which assembles the
game and invokes `GameServer::startServer`. The server constructs and initializes its
`NetServer` before selecting and validating a map. A no-bot session still opens a real
network listener.

The selected map reaches `GameServer::loadPath`, then `GameLogic::load`. Loading mounts
content, executes the level's console scripts, initializes game-mode Python, and publishes
world state through the engine's event and object systems. The order is observable: script
commands refer to registered types and resources, while synchronous load events can invoke
other subsystems before loading returns.

`BF2::run` delegates each iteration to `BF2Engine::mainLoop`. Timing supplies the server's
simulation step count and timestep; `GameServer::update` coordinates connection work,
simulation, events, and outgoing state. Loading also pumps a special update path, so seeing
an update call alone does not prove that normal simulation ran.

Shutdown includes game unloading, owner and reference release, listener shutdown, and
receive-thread completion. A successful startup test therefore also checks clean shutdown,
immediate port reuse, and reload. Record the configuration and paths exercised with every
test result; the presence of these methods does not establish runtime coverage.

## Client menu disconnect

The stock `mainMenu.swf` disconnect page queues `dice.bf2.Logic.disconnectGame`
through its UpdateManager, then polls `Logic.isGameRunning` while displaying the
disconnect modal. A missing native callback leaves the game running and the page
waiting indefinitely. `SwiffPlayer.cpp` registers the callback in the rebuilt plugin.
It requests shutdown on the player's host while the separately looked-up registry
host owns the busy cursor. The registry host may be absent; the player and its host
are required. Arguments are ignored and the ActionScript result is left untouched.

This bridge follows the original x86 BF2 1.50 `SwiffPlayer.dll` (MD5
`b4ef39939007a555f23829824c021e35`): registration at `0x1004DFC3` binds the
callback at `0x1004CB10`; `0x10013520` establishes the cursor/busy scope. The callback
invokes host slot `+0xB4`. In BF2.exe, `SwiffHost::requestShutdownGame` at `0x491860`
queues event 17, and `BF2Engine::renderMenu` at `0x40BD10` consumes it through
`shutdownGame(false)`. `SwiffHost::isGameRunning` at `0x486C90` reports whether
the global game still exists. The movie clears its modal after that becomes false.
The movie library is client-only; there is no dedicated-server ELF counterpart to
this callback. No host layout, event ID, or network protocol changes are needed.

`Code/Tests/swiff_disconnect_runtime.cmd` resolves the registered ActionScript
member instead of calling `SwiffHost::requestShutdownGame` directly. It takes a
fresh native plugin object directory and an isolated test output directory. The optional
`native` argument remains accepted. Distinct registry and player hosts expose
incorrect cursor ownership; the callback must preserve call order and the
caller's result even when the host returns false. This boundary check does
not establish joined-client teardown or reconnect stability. The existing
`swiff-native-as-host`, `swiff-host-bodies`, and `RW-88` frontiers outside this
callback remain subject to their existing recovery and validation requirements.

## Object-owned sounds

Weapon components request `Fire1P`, `Fire1P_Outdoor`, `Fire3P`, and their other
samples through `SimpleObjectTemplate::createSoundFromTemplate`. The client factory
looks up `S_<object-template-name>_<sound-name>` with sound class ID 9986001,
requires an engine template and a nonempty filename, then passes that template and
the owning object to `BfSound::createSound`. That facade assigns the sound's owner
and obstruction evaluator. The same factory serves vehicle, projectile, soldier,
and ambient object sounds.

The original BF2 1.50 x86 BF2.exe client implementation is `0x624A20` (image MD5
`4c843dcee79388341f72c3a7adb1ace6`); weapon initialization at `0x608920` calls it for
all eleven weapon sound slots. Its four `impl()` calls and
two filename reads are preserved, as are the runtime dedicated-mode and missing
sound-facade guards. The symbolized dedicated ELF implementation at `0x081F2760`
returns null without looking anything up. The shared source now selects that
server implementation only for `DEDICATED=1`. Previously, the server body was also
used in the client, leaving every weapon sound slot empty before firing reached
the audio engine. This recovery changes no interface layout or sound-file format.

`Code/Tests/weapon_sound_runtime.cmd <isolated-output-directory> client` checks the
factory guards, owner forwarding, all eleven weapon sound slots, single and
automatic third-person firing, stopping, and sound release. Run it again with a
different output directory and `dedicated` to check the server's null factory.
The fixture copies the selected production bodies verbatim under their real
headers and compiles the actual weapon sound component; its surrounding observers
provide no audio device or game world. `BF2_TEST_FACTORY_SOURCE` can select a
pre-fix source snapshot to verify that the client regression fails before the fix.

Factory and weapon-component checks establish sound creation and playback dispatch;
they do not establish audible output from a live joined client. Device playback,
first/third-person mixing, and repeated gameplay still need runtime validation.
Existing `bfsound-debug-draw` frontiers and the opt-out `BF2AUDIO_NO_VORBIS` decoder
frontiers are outside this factory and remain unchanged.

## Content and the console

`.con` files are executable engine configuration. The console binds command names such as
`ObjectTemplate.*`, `Object.*`, and `sv.*` to typed engine operations. Templates describe
shared object definitions; instantiated world objects carry live state. Registration and
load order determine which names and resources a script can use.

`FileManager` resolves loose paths and mounted archives. The working directory, mod path,
archive order, and level directory all affect lookup. Diagnose a missing command or resource
at its registration or loading boundary before adding fallback behavior. Python game modes
use the embedded interpreter and native bindings; a fixture that bypasses those scripts
does not validate production content loading.

### The `openbf2` console extension

`openbf2.*` is an OpenBF2 extension, not a recovered command; no retail image defines it. It is
`[CHOSEN, not recovered]` under [issue #39](https://github.com/B2BF/OpenBF2/issues/39).
`Game/Main/OpenBF2Console.cpp` registers it from `ModuleBF2::init`.

`openbf2.addModDll <file>` loads a native module at run time: either an OpenBF2 plugin that exports
`initDll(IClassManager*)` (initialized through the same handshake as `Core/PluginDll.cpp`, a false
return unloads it) or a module that does its work in `DllMain` and exports no `initDll`, which is
kept loaded. A bare name resolves to `.\<file>` on Windows and `lib<stem>.so` on Linux. The command:

- exists only in the dedicated-server image; it compiles out of the client and editor, so a
  downloaded map cannot reach it on a player's machine;
- refuses any call dispatched from a mounted-archive script (`OldConsole::isDispatchingArchiveScript`),
  so bundled map or mod `.con` files cannot load native code. Operator callers — a loose server
  `.con`, rcon, Python, or the server console — are allowed.

A module's `DllMain` runs on load, so an integration that patches the host from `DllMain` takes
effect without an `initDll`. bf2hub is such a module: its `DllMain` patches the engine's `dice_py`
import slots (`Py_InitModule4`, `PyImport_ImportModule`) so that its `bf2hublib` and `bf2.bf2hub`
Python modules are injected when the engine registers its `host` module and imports `bf2`. This only
takes effect when the module is loaded **before** Python initialization — load it from
`ServerSettings.con` or `Init.con`, which run before `GameServer::init` calls
`pythonHost()->initialize()`, not from a map script that runs afterward.

This loader does not itself reroute sockets. bf2hub's GameSpy/master-server redirection relied on an
install-time patch of the host's import table that a runtime load does not reproduce; point the
engine at the correct master-server address in source instead.

## The editor

The editor targets this engine's 1.50 content contract. Its application, UI, command history,
and editing modules are new code. It shares engine code with the client and isolates
editor-specific changes in shared translation units with `BF2_IMAGE_EDITOR`.

Lightmap generation is shared renderer code recovered from the 1.50 RendDX9 image: the terrain's
light sampler (`Geom/TerrainLightSampling.cpp`), the static-mesh light sampler
(`Geom/LightmapGeneration/MeshLightSampling.cpp`) and the generator (`Geom/LightmapGeneration/`).
1.50 never creates the generator. Only the editor's renderer flavour (`BF2_RENDDX9_EDITOR=1`)
publishes the 2006 editor's `lightmapGeneration` console object over it, using the 2006 binding
names (`init`, `traceTerrain`, the settings and `light.*` rows). This is chosen behaviour, not
recovered 1.50 behaviour. Both traces are recovered: the intensity trace (the shipped default) and
the colour trace (`enableIntensityLightmaps 0`), which writes RGB lightmaps from what each light's
views see over the ambient colour. The colour trace has not yet been compared with a retail-editor
golden.

An object is traced from its mesh's precomputed sample grid: `<template path>/Meshes/<template
name>.samples` for geom 0, lod 0, otherwise `.samp_<geom><lod>` (the SMP1 and SMP2 layouts and an
older one without a magic word). No stock archive ships these files; the art exporters write them.
A template with `lightmapOverride` caps the grid at its `setLightmapSizeAll` size. The sampler writes
the object's lightmap, dilated and converted to DXT1 (DXT5 when `lightmapFormat` is not 1) with mips,
as `<Editor_LevelSavePath>/<GSLevelDirectory><GSLoadLevel>/Lightmaps/Objects/<template>` plus the
position suffix the game's loader expects. Outside the renderer's editor mode the game reads object
lightmaps only from the level's atlas (`Lightmaps/Objects/LightmapAtlas.tai`). The 1.50 rows `StaticMeshRenderer.loadAllLightMapFilesDLL
<directory> <extension>` (one atlas, named `LightmapAtlas`) and `StaticMeshRenderer.loadAllLightMapFiles
<directory> <extension>` (numbered batches `LightmapAtlas0`, `LightmapAtlas1`, ...) pack a directory's
lightmaps through TextureAtlasBuilder.dll, reading and writing under `Editor_LevelSavePath`; the
directory is the level's object-lightmap directory relative to that root
(`<GSLevelDirectory><GSLoadLevel>/Lightmaps/Objects/`) and the extension `.dds`. Both
bind in the native and editor flavours, as do `GeometryTemplate.setLightmapSizeAll` and
`Object.geometry.GenerateLightmapForThisInstance`; the retail-compatible flavour publishes no console
rows. The editor mounts `bf2editor/LightmapSamples*.zip` at `Objects` when the object archives are
mounted, as the retail editor's setup did. Its object trace starts from the level's own object
lightmaps - the level's atlas cut back into single lightmaps (`Framework/LightmapAtlas`), then the
single lightmaps the editor kept in `Editor/Lightmaps/Objects` - so the re-packed atlas keeps every
object the trace does not reach; stock levels ship only their atlas. The re-packed atlas goes into
`Lightmaps/Objects` and the traced single lightmaps into `Editor/Lightmaps/Objects`.
The game's 1.50 `StaticMesh.fx` comments out its include of the lightmap-generation techniques the
trace draws static meshes with as occluders; the editor's flavour compiles it with that include
(`RendDX9/Effect.cpp`), as the retail editor's own shader did, so terrain lightmaps carry the
objects' shadows. This is chosen behaviour for the editor; the game images are unchanged.

Editable roads are shared renderer code recovered from the 1.50 RendDX9 image (`Geom/RoadEditable.cpp`,
`Geom/RoadTemplate.cpp`, `Geom/RoadEditableRenderer.cpp`, `GeomMisc/EditorSpline.cpp`,
`GeomMisc/PerlinNoise.cpp`); 1.50 binds no console rows for them. Their interfaces are declared in
headers the renderer and the editor both include (`Geom/IRoadEditable.h`, `Geom/IRoadTemplate.h`,
`Geom/IRoadEditableRenderer.h`, `GeomMisc/IEditorSpline.h`, `GeomMisc/IPerlinNoise.h`), whose strings
are the renderer's host string in its image and `std::string` in the editor's. The editor's road
module (`App/RoadNetwork`) makes the objects through the class manager, keeps them alive (a road holds
no reference to its spline, template or noise) and implements the retail editor's road rows over them.
In the editor's renderer flavour `RoadEditable` takes its buffers out of the buffer manager when it
drops them: the shipped body releases a dropped vertex buffer a second time on its next rebuild, which
the retail Memory.dll heap absorbed. This is chosen behaviour for the editor; the game images keep the
shipped body.

The editor's `App/` layer owns the window, engine host, editing session, and renderer
integration. `Framework/` supplies command history, selection, settings, file readers and
writers, and source-control interfaces. Editing tools live with the application. Its framework
checks run without starting the engine, while viewport and content tests need a game
installation and the rebuilt editor renderer.

The editor uses Dear ImGui and ImGuizmo for its interface. Its renderer and texture atlas
DLLs are built separately from the client's plugin outputs. The source-control interface
registers `NullSourceControl` by default; editing does not require Perforce. Console
bindings expose editing operations through the same session used by the UI; see the
[editor console](editor-console.md) for commands and replay behavior.

Retail-compatible files and `.con` semantics are the editor's compatibility boundary.
Compare output with retail-editor golden files. An unfinished operation reports its failure;
it must not claim to have saved or compiled content. Follow the [editor build steps](building.md#editor)
for its dependencies, checks, and output directories.


Every editor save is planned before anything is written (`Framework/SavePlan`): each part of the
level - its object and settings files, the gameplay layers, templates, materials, animation systems,
descriptions, painted maps, compiled terrain, overgrowth, roads, lightmaps - adds the files it would
write and the step that marks it saved, without writing. The plan can be shown first (File > Save,
`EditorLevel.preview`, with a line diff of each text file). Writing it puts each new file beside its
target, copies every file it replaces into the level's backup folder
(`bf2editor/Backups/<mod>/<level>/<date and time>/`, with a manifest), and only then replaces the
targets; if one cannot be replaced, the ones already replaced are put back, so a save never leaves the
level half written. A part that cannot be saved (a file from a mounted archive, one source control
refuses) is reported and left unsaved; the other parts are written. A backup can be put back with
every save after it (`EditorLevel.restore`), which is itself a backed-up save. The retail editor wrote
each file as it went and kept no backup; this is chosen behaviour for the editor.

The HUD editor is an addition; the retail editor left HUD scripts to a separate tool. Unless started with
`+hud 0`, the editor runs the client's own HUD start-up: the HUD's modules, the `HudManager` singleton the renderer
takes its paint hook from, and `BF2HudManager::init`. The renderer then updates and paints the HUD
itself. `App/HudEditor` decides whether the HUD is drawn through the renderer's retail
`renderer.drawHud` row: in soldier mode, or while the panel previews it. A preview holds the
selected part's show variables and runs the HUD's widget update, which the game runs only for a player
in a vehicle. `Framework/HudSetup` keeps the scripts `HudSetupMain.con` runs as lossless lines and
tracks hudBuilder's selection across them, so every row belongs to its node. An edit is a line edit
through the undo history. A row the builder can safely take again is replayed on the live HUD; a
failing builder row ends the process, so the editor checks the live object and the row's words first.
`Bf2TransformNode` reports where each node is updated, so the view's outline follows the game's own
layout; that hook is compiled only into the editor's copy of the file. The save rewrites the menu
archives that hold the scripts (`Framework/ArchiveRewrite`). The engine keeps its archives open, so a
save part may unmount them just before the save plan replaces its targets and mount them again
afterwards.