# c-input-script: scripted input for unattended client runs (join, spawn, bots)

[CHOSEN, not recovered] Test tooling on `mac/c-input-script` (from `mac-metal` 481c5fe). Every
change is a macOS arm or a `[TEMP]` switch that is off by default; the MSVC x86 paths are
unchanged. Intel i7-8569U, macOS 15.7, Apple clang 17.

## Result

The loopback harness now drives the macOS client from the loaded-map screen to a spawned
soldier with no person at the keyboard:

`MAP LOADED ... PRESS ESCAPE` -> Escape -> spawn screen -> kit 0 -> spawn point (map click) ->
DONE -> spawned (in-game HUD, minimap, ammo counter, mouse look turns the view).

```sh
tools/macos/loopback/loopback.py --build <tag> --client-seconds 130 --input-script join-and-spawn [--ai --bots 16 --ai-trace]
```

For client-hosted singleplayer, use `--no-server --input-script sp-start`.
`--no-join` alone leaves the dedicated server running on the SP host's port.
See [Single player runs](#single-player-runs).

The world pass draws without switches
(`BF2_METAL_IGNORE_FOG` is gone; `BF2_METAL_SKIP_REFUSED_DRAWS=1` remains an off-by-default debug aid).

## Harness overview

`tools/macos/loopback/loopback.py` (run with `/usr/bin/python3`; options in `--help`) starts a
sandboxed dedicated server and the macOS client on one machine and joins them:

- It refuses to start when UDP 16890 or 29911 is taken and never binds or probes 16567. A
  machine-wide lock serialises runs; queueing is normal.
- Game content is cloned (`cp -Rc`) from the read-only install into
  `out/loopback/run-*/server-root` and `client-root`; the server gets offline settings
  (`serverPort 16890`, `internet 0`, `gameSpyPort 29911`, VoIP/rank/unlocks/recording off).
- The server runs under `expect` (`server.tcl`) and `sandbox-exec -f server.sb`; the client runs
  under `sandbox-exec -f client.sb` with `HOME=<run>/client-home`, a fresh profile and a
  placeholder `Registry.con`. Logs: `client.log`, `client.result`, `server.pty.log`; crash
  reports are copied with their top frames. `--lldb` runs the client under lldb in the same
  sandbox.
- `client.sb` allows outbound traffic to `localhost:16890` only: no LAN broadcast, no GameSpy,
  no DNS.

## Single player runs

Client-hosted single player needs no dedicated server (`--no-server`, sandbox profile
`singleplayer.sb`). The client binds 127.0.0.1:16890 only about 10 s after launch, when PLAY is
clicked, so `sp-run.sh` holds the same machine-wide lock as `loopback.py` and waits for free
ports for the whole run. The scripts wait for `mods/bf2/Movies/menu.bik` because the runner
passes `+restart 1`, and are run with `BF2_AI_SPAWN_ONLY=1`:

- `sp-instant.txt`, `sp-instant-play.txt`: welcome screen -> PLAY SINGLEPLAYER (instant battle).
- `sp-menu-start.txt`: SINGLEPLAYER page -> Strike At Karkand, VETERAN -> START (needs
  `BF2_REUSE_HOME` with an offline account).
- `sp-leave-replay.txt`: leave the game and start a second one.
- `sp-start.txt`: from a fresh offline profile through the SP page to a spawned Assault soldier.
- `sp-quit-after-map.txt`, `sp-quit-in-map.txt`: Dalian Plant from a fresh profile, then the QUIT
  dialog from the main menu after DISCONNECT, or from the in-game menu. `sp-quit-check.sh BUILD_DIR`
  runs both with 4x antialiasing and fails unless the client exits with status 0 and without the
  RendDX9Init.cpp:738 device release fatal (the quit runs the renderer deinit; the window close and
  the application menu's Quit end the process with `exit(0)` before it, as on Win32).

## The input script (`BF2_INPUT_SCRIPT=<file>`)

The engine is `Platform/InputScript.{h,cpp}` (shared with the Win32 host below; the macOS key
table is `Platform/macOS/MacInputScript.mm`), hooked into `CocoaHost::Impl::pump`. One script tick runs
per host pump (one per main-loop frame, and during the loading screen). Actions become the same
`HostKeyEvent` / `HostTextEvent` / `HostMouseMoveEvent` / `HostMouseButtonEvent` /
`HostMouseWheelEvent` / `HostRawMouseEvent` values the NSEvent handlers build, delivered to the
same listeners: the window-message source (`BF2Window_macos.cpp`: Swiff menu, console, chat;
`WM_KEYDOWN`/`WM_CHAR`/`WM_MOUSEMOVE`...) and the input devices (`MacKeyboardDevice`,
`MacMouseDevice`: control maps, HUD pointer, mouse look). `captured` follows the real rule
(relative mode with focus), so menu and in-game paths split exactly as for real input; a relative
move is also sent as raw (`GCMouse`) motion when a `GCMouse` is connected, as a real mouse does.

While a script is active (only then): focus is reported as held, real keyboard/mouse/GCMouse
input is ignored, the app is not activated (the user's focus is not taken), and the real cursor is
never hidden, warped or disassociated. Without the variable nothing changes.

`BF2_INPUT_SCRIPT_REAL_FOCUS=1` ([TEMP]) keeps the scripted input but drops the forced focus: the
app is activated when its window is created, and focus changes made from outside (for example
`osascript -e 'tell application "Finder" to activate'`) reach the engine as WM_ACTIVATEAPP /
WM_ACTIVATE / WM_KILLFOCUS. This is how the full-screen tab-out is tested unattended
(`scripts/tabout.txt`). With `BF2_HOST_TRACE=1` each transition is
logged as `[host] focus key=<0|1> active=<0|1> fullscreen=<0|1>`.

| Command | Meaning |
| --- | --- |
| `wait-log TEXT` | wait until TEXT appears in the client log after the previous match (the file stdout is redirected to, or `BF2_INPUT_SCRIPT_LOG`); the script's own lines never match |
| `wait-log-else S LABEL TEXT`, `label LABEL` | as `wait-log`, but after S seconds without a match jump forward to `label LABEL` (c-options: the intro movies play only without `+restart 1`) |
| `wait-frames N`, `wait-ms N` | wait N pumps / milliseconds |
| `timeout S` | wait-log timeout (default 180 s); on timeout the script stops and says so |
| `key NAME [holdms]`, `key-down NAME`, `key-up NAME` | key by position: `IDKey_` names without the prefix (`escape`, `enter`, `grave`, `a`, `f1`, `leftshift`...) or `0xNN` (kVK). A press carries the characters NSEvent would (current layout) |
| `type TEXT` | each character as the key (and Shift) that produces it on the current layout |
| `console CMD` | Grave, `type CMD`, Enter, Grave: the shipped console |
| `move X Y`, `click X Y [button]` | absolute pointer in back-buffer pixels (menus, Swiff) |
| `move-rel DX DY`, `button-down/up B`, `wheel N` | relative motion (mouse look), buttons, wheel notches |
| `pointer X Y`, `hud-click X Y [button]`, `pointer-scale K` | the in-game HUD pointer: homed to the top-left by a large relative move, then moved by X/K, Y/K counts (K = 1.2 HUD pixels per count, measured with the shipped `Controls.con` and `hudManager.setPointerMouseSensitivity 40`) |
| `capture LABEL` | writes the next presented frame as `present_<n>_<LABEL>.png` (needs `BF2_PRESENT_PNG`) |
| `log TEXT`, `quit`, `end` | report line; request quit (as the close button); stop |

Reports go to stderr as `[TEMP] [INPUT-SCRIPT] t=<s> frame=<n> ...`.

## Win32 host (BF2.exe, x86 and x64)

[CHOSEN, not recovered] The same engine and script format drive the rebuilt Windows client, so
it can join and play without anyone at the guest's keyboard or mouse (in a VM, mouse input
synthesized inside the guest does not reach the game's DirectInput mouse).
`Platform/Windows/WinInputScript.{h,cpp}` holds the host and its key table; key names map to
`DIK_` codes (scan codes) instead of `kVK_` codes. Only when `BF2_INPUT_SCRIPT` is set:

* `BF2::handleInGameWinMessages` runs one tick per pump and drops the queued real keyboard and
  mouse messages (`WM_KEYFIRST..WM_KEYLAST`, `WM_MOUSEFIRST..WM_MOUSELAST`, `WM_INPUT`).
* Keys are sent to the game window procedure as `WM_KEYDOWN`/`WM_KEYUP` plus the press's `WM_CHAR`
  sequence (Escape carries 27, Enter 13); they bypass `TranslateMessage`, so the real keyboard
  state adds nothing. Characters come from `ToUnicode` with the current layout.
* Pointer moves, buttons and the wheel become `WM_MOUSEMOVE`, `WM_xBUTTONDOWN/UP` and
  `WM_MOUSEWHEEL` at the script pointer only while a menu (`menuLogic`) is up; in game the
  exclusive DirectInput mouse receives no window messages, as with a real mouse.
* `KeyboardDevice::update` and `MouseDevice::update` take their buffered device data
  (`DIDEVICEOBJECTDATA`: `DIK_` offsets with 0x80/0, `DIMOFS_X/Y/Z` deltas, `DIMOFS_BUTTONn`) from
  the script feed instead of `GetDeviceData`; `activate` counts the device as acquired even when
  `Acquire` failed (the window need not be in front) and starts it with an empty feed, as a real
  buffer starts at acquisition. `Window::getHasFocus` reports focus.
* `capture LABEL` writes the game window's client area (a screen copy, else `PrintWindow`) as
  `present_<pump>_<LABEL>.bmp` into the directory `BF2_PRESENT_PNG` names.
* `wait-log` reads the file stdout (else stderr) is redirected to (`GetFinalPathNameByHandle`),
  or `BF2_INPUT_SCRIPT_LOG`.

Script for the Boot Camp guest: `tools/windows/input-scripts/play-combat.txt` (join-and-spawn
expanded; the level-load marker is `postLoad #1`, as the Win32 log has no Meme texture trace).

### dinput8.dll proxy (retail BF2.exe, or the rebuilt client without the built-in host)

`tools/windows/dinput8proxy/` ([TEMP] test tool; `build.cmd x86|x64 OUTDIR`) builds a dinput8.dll to
put next to a *copy* of BF2.exe. With `DI8PROXY_SCRIPT=<file>` it wraps IDirectInput8A and the
system mouse and keyboard: scripted `GetDeviceData`/`GetDeviceState`, `Acquire` reported without
focus, keys and `click X Y` posted to the game window. The script format is this one without the
commands that need game knowledge (aim, face, walk-to, type, console); `wait-log` reads
`DI8PROXY_LOG`. Reports go to `DI8PROXY_REPORT`, captures to `DI8PROXY_CAPTURE_DIR`. Without the
variable every call is forwarded to the system dinput8.dll. A retail copy outside the install
directory runs without its HIGHDPIAWARE compatibility layer (a window twice the size); posted
coordinates are client coordinates and are unaffected. `retail-spawn-play.txt` joins, spawns,
walks, turns, looks up and fires.

The aim / face / walk-to query in `BF2Engine.cpp` is shared. Without the variable the Win32
client behaves as before: every hook tests `winInputScriptActive()` first.

## The shipped way from JOIN GAME to spawned

* **Loaded-map screen**: Escape (the Swiff menu's own key) enters the game; the JOIN GAME button
  (bottom right, 745,583) is the same action.
* **Spawn screen** = BF2HudManager mode 1, built from `Menu_client.zip`
  `HUD/HudSetup/SpawnInterface/HudElementsSpawn.con` in 800x600 HUD space. Its buttons run console
  commands: `SelectKit0..6` -> `spawnManager.setPlayerKit N` (kit 0 at 10,73 246x69),
  `DoneButton` (666,539 124x17) -> `hudManager.setDone 1`, `SelectTeam1/2` ->
  `spawnManager.setPlayerTeam`, `SuicideButton` -> `spawnManager.commitSuicide`.
* **Console**: the input path reaches it (`console-check` script: Grave opens it, typed text
  appears, Enter executes), but the shipped client console answers `Unauthorised method!` to
  `spawnManager.setPlayerKit 3` and the other HUD commands: only the HUD buttons may run them. So
  there is no console route to a spawn; the buttons are the shipped way.
* **Spawn point**: no console command. A press on the map in spawn mode runs
  `Bf2MenuMapNode::pickSpawnGroup`, which takes the nearest active spawn group of the player's team
  within the icon pick radius (`m_field730 / zoom`). `hudManager.setDone 1` then sends
  `selectSpawnGroup(selected)` (PostRemote 6); the server's `spawnPlayer` refuses group 0.
* **Map visibility**: on first entry the spawn screen's map is often hidden (mode 1 does not set
  `MapShow`; only leaving a mode through the default arm does). Enter (`c_GIEnter`) twice closes and
  reopens the spawn screen and shows it, as a player would do.
* **The HUD pointer is relative**: in game the mouse is acquired exclusively and
  `BF2HudManager::handleGameInput` moves its own pointer by the mapped mouse axes (`c_GIMouseLookX/Y`
  x sensitivity), clamped to 800x600. Absolute window coordinates do not reach it; hence
  `hud-click`.
* **Controls**: the harness profile used to have an empty `Controls.con`; the profile load then
  left the mouse axes unmapped and the HUD pointer never moved. The harness now copies the shipped
  `mods/bf2/Settings/Default/Controls.con`, which is what a profile the game saved holds.

`tools/macos/loopback/scripts/join-and-spawn.txt` does exactly this (kit by click, spawn by map
click, DONE by click; Escape and Enter by key; mouse look by relative motion).

## Changes

| Commit | What |
| --- | --- |
| `0cfc0f3` | input script, CocoaHost hook, labelled present captures (`MetalBridge.mm`, weak default) |
| `a32eb10` | `pointer` / `hud-click` |
| `5a0b70b` | native effects: an immediate vector for a shader-constant state (`TerrainShader_Low.fx:45 PixelShaderConstantF[1] = vec4(0.5,0.5,0.5,1)`) was refused, the terrain effect was cached unbound, and the first in-game frame stopped in `Effect::begin` (`Low_Terrain`) |
| `7e2cad3` | Metal: the unmapped render-state stop names the state; `[TEMP]` `BF2_METAL_IGNORE_FOG=1` and `BF2_METAL_SKIP_REFUSED_DRAWS=1` (both off by default) |
| `a9cffb2` | server: `ServerSettings::getNumPlayersNeededToStart` returned a fixed 2 (initial import); retail (ia-32 0x080BC1F0) returns the configured value (ranked: 6, or 8 above 32 players) |
| (this doc) | `loopback.py`: `--input-script`, `--present-png/--present-every`, `--client-env`, `--ai-trace`; default `Controls.con` in the profile; scripts `join-and-spawn`, `explore`, `calibrate` |

## Stops on the way (in order)

| # | Stop | Class | Status |
| --- | --- | --- | --- |
| 1 | `[FRONTIER] Effect::begin: the D3DX effect is unbound` (`Low_Terrain`) after Escape: native FX refused `PixelShaderConstantF[1] = vec4(...)` | native effect compiler | fixed (`5a0b70b`) |
| 2 | `[FRONTIER] IDirect3DDevice9::Draw state unmapped render-state value` = render state 28 `D3DRS_FOGENABLE` = 1; `Device::drawShared` <- `RaShader::draw` <- `RenderQueue::drawWalkNear` <- `flushWorldPass` <- `Renderer::renderFrame` <- `BF2Render::render` <- `BF2Engine::mainLoop` | Metal backend: fixed-function fog after shaders | open; `BF2_METAL_IGNORE_FOG=1` draws fogless |
| 3 | `DrawIndexedPrimitive (sampler 2 texture dimension differs from the shader's)`, 8192 primitives, `RaShader::draw` <- `drawWalkDefault` <- `renderQueueFlush(40)` <- `renderFrame` | Metal backend / texture binding (terrain: the terrain is black in captures) | open; skipped with the switch |
| 4 | `Draw inside an occlusion query interval [RETIRE-WHEN: macos-g4-visibility]` | Metal backend | open; skipped with the switch |
| 5 | Round never started ("1 more player(s)") with one human | server fidelity (`return 2`) | fixed (`a9cffb2`); note the client HUD text still counts 1 because its `getIsConnected` count is 0 on the client side |

## Runs (validation)

All runs: `loopback.py --build c-input-script`, native effect compilation (no Wine), 800x600,
`BF2_METAL_IGNORE_FOG=1 BF2_METAL_SKIP_REFUSED_DRAWS=1`.

| Run | Result |
| --- | --- |
| conquest, `join-and-spawn` (`out/loopback/run-20261101-063243`) | level loaded at t=~60 s, spawn screen, kit 0 selected, spawn point selected ("PRESS DONE TO SPAWN"), DONE: **spawned** at Construction Site (in-game HUD, minimap, 30 rounds); `move-rel 300 0` turned the view. Client ran to the 130 s timeout, no crash, no new frontier. Captures: `present_02323_spawned-1.png`, `present_02483_spawned-2.png` |
| coop, `--ai --bots 16 --ai-trace` (`run-20261101-063659`) | the human spawns ("INSTANT SPAWN", minimap at Reactors); the **server aborts** at once at `[FRONTIER] dice::hfe::ai::AIEventListener::handleEvent - category 3 id 1 - player entered vehicle` (`AI/AIInterface/Main/AIEventListener.cpp:289`), chain `ServerGameLogic::spawnPlayer` -> `Soldier::enter(IPlayer*)` (`Soldier.cpp:3089`, `postEvent(3, 1)`) -> `AIEventListener::handleEvent`. It is the human's own spawn, so no bot spawned: `BF2_AI_TRACE` printed nothing (0 spawns, 0 ticks). Posted to `coord/ai-requests.md`. No crash report (sandboxed abort); the chain is static |
| `console-check` (`run-20261101-064315`) | console opens, text typed, `Unauthorised method!` |
| `calibrate` (`run-20261101-062204`) | HUD pointer: +100,+50 counts -> (120,60) px; +200,+100 -> (241,122): 1.2 px per count |

Other checks: `bf2_client` (renderer ON, native FX linked with
`-DBF2_FX_NATIVE_DEPS=<nativefx build>`), `bf2`, `bf2_aidll`
build; `bf2_vtable_audit` passes; `Code/Tests/macos_input`: 254 checks, 0 failures (script off).

## Open items

1. **AI**: the `AIEventListener` category 3 arms (id 1 now; 0/2/3 are the same family) for the
   coop run; then rerun `--ai --bots 16 --ai-trace` to see the first bot spawns.
2. **Metal world pass**: fixed-function fog after shaders (`D3DRS_FOGENABLE`), the sampler-2
   dimension mismatch on the terrain draw (black terrain in the captures), and draws inside
   occlusion queries. Until then the two `[TEMP]` switches are needed for in-game runs.
3. The client HUD keeps "The game will start when 1 more player(s) join": `HudInformationLayer`
   counts `getIsConnected()` players on the client, which is 0 there; not checked further.
4. `pointer-scale` depends on the profile's mouse sensitivity; the harness profile uses the
   shipped default. A different profile needs `calibrate` again.
5. Retire `BF2_INPUT_SCRIPT` (`[RETIRE-WHEN: client-test-automation-replaced]`, macOS and Win32)
   when a real test-automation path replaces it.

## Fixed spawn pose (`loopback.py --pose`)

[TEMP] play-test fixture. Spawn points are random, so the same script starts somewhere else on every
run. `--pose X/Y/Z[/YAW[/PITCH[/ROLL]]]` copies `tools/macos/loopback/pose/playtest_pose.py` into the
cloned server root (`mods/bf2/python/game/`), imports it from `game/__init__.py` and moves every human
(non-AI) player to that world position on each spawn (PlayerSpawn handler, `soldier.setPosition`).
`--pose log` only records spawn positions, for picking a pose. The module logs to
`<run>/server-root/mods/bf2/python/game/playtest_pose.log`, because the embedded interpreter does not
route `print` to the server console.

- It uses only the retail server Python API, so the same file works on the retail dedicated server, run
  from a copied server root (never inside the original game directory).
- The position holds: the client's `[PLAY] pos=` lines report the pose after the spawn
  (run-20261105-171927, -172323).
- The view direction does not follow `setRotation`: the client owns its view. For a fixed view, steer it
  in the script with `aim X/Y/Z <tolerance> <ms>` (a world point) after the spawn.
- The handler is registered once at import. Map changes (`--map2`) are not verified yet.
- For fully repeatable frames, also take the bots out (`--bots 0`) or keep them still (`--ai-spawn-only`).

## Standard scenarios (`loopback.py --scenario NAME`)

```sh
tools/macos/loopback/loopback.py --build-dir out/build/<tag> --scenario infantry-range
```

[TEMP] play-test fixture. A scenario is a directory `tools/macos/loopback/scenarios/<name>/` with:

- `pose.cfg`: the server half. It is installed as `playtest_pose.cfg` next to `playtest_pose.py`.
- `script.txt`: the input script. `include NAME` finds files in the scenario directory first, then in
  `scripts/`.
- `loopback.args`: the default harness options (map, bots, `--spawn-time 1`, client seconds). Options
  given on the command line override them.
- `README`: one line saying what the scenario checks.

`--scenario` also accepts a directory path, which is useful for trying out a new scenario.
`--build-dir` runs a build from another worktree (`out/build/<tag>` or its `bin_macos/`).

The scenario file (`playtest_pose.py`) uses only the retail server Python API, so the same pose files
work on the retail dedicated server, run from a copied server root:

| Line | Effect |
| --- | --- |
| `player X Y Z [YAW]` | Every spawn of a human player is moved here (`soldier.setPosition`). |
| `pose NAME X Y Z [YAW]` | A named spot. A human writing `goto NAME` in chat is moved there, so one run can visit several spots. |
| `bot friend\|enemy\|team1\|team2 X Y Z [YAW]` | The first AI player of that team to spawn (the next one for each further line) is placed here. This happens on every spawn of that bot and on every human spawn. `friend` and `enemy` are relative to the first human's team. |

- **Logs.** `<run>/playtest_pose.log` (copied out of the server root at the end) records spawns,
  placements, `[goto]`, `[kill] victim killed by attacker with weapon`, and `[bot]` health and position
  changes of placed bots, once a second (`bf2.Timer`).
- **The client owns its view.** `setRotation` turns the server soldier but not the view. Scripts steer the
  view with `aim X/Y/Z` at fixed world points.
- **Heights.** Give poses at the soldier's rest height: terrain plus about 1 m, the `[PLAY] pos` value. A
  pose that is too high plays a short fall.
- **Bad teleports.** A pose inside or under static geometry is pushed away by the physics. Seen in
  explore runs: from -195/-70 the soldier ended at -167.5/-16.5, and from -165/190 at -165/94.2. Every
  pose below was checked: the client's `[PLAY] pos` equals it.
- **Chest height.** Aim at a still bot's chest at its root y minus 0.2 m. The input script's
  `aim enemy|friend` uses root + 1.2 m, which passes over a standing bot's head at 25 m (`[TEMP]` query in
  `BF2Engine.cpp`, not changed here).
- **Waits.** The scripts wait for concrete events:
  - after a `capture LABEL`, `wait-log LABEL.png` (the `BF2_PRESENT_PNG: wrote ...` line);
  - for spawn and death, `wait-log-else S L [PLAY] alive=1|0` (`BF2_PLAY_TRACE`);
  - after a teleport, `wait-log-else 5 L pos=X/`.

  A client without these traces (the Windows port, if it lacks them) falls through after the
  `wait-log-else` timeout. The menu and HUD transitions print nothing, so they keep short fixed waits.
- **Scene-only captures (render views).**
  - The render-view scripts switch the HUD off with `console renderer.drawHud 0`. This hides the
    minimap, ammo, chat and kill messages.
  - In retail, `renderer.drawHud` has the player console's access level 1. The other `renderer.draw*`
    switches (`draw1pMeshes`, `drawNametags`) are access 2, so retail answers `Unauthorised method!`
    (`RendererConsole.cpp`: `SetConsoleClassDefaults("renderer", 2, 0)`).
  - So the first-person weapon stays: the scripts draw the pistol (`key 2`), the smallest and steadiest
    model. Comparisons leave out its corner: the region is y < 340, plus x < 520 for y >= 340, at
    800x600.
  - Control-point markers (`[75m]`) are drawn by the renderer and stay.
  - Chat needs the HUD, so each `goto` switches it on for the teleport and off again.
- **Repeatable view direction.** `aim` steers by mouse counts. Its last correction can land after it
  reports `reached`, because the view follows the input a few frames later; in one coordinator run this
  turned the view 2.7 degrees further.
  - The render views therefore aim in three steps: tolerance 0.5, then 0.15, then 0.08 degrees, with
    250 ms between steps.
  - One count turns the view about 0.15 degrees, so 0.08 asks for the nearest count.
  - After each capture a 300 ms `aim` reports the remaining error. It was 0.0-0.1 degrees in both runs
    below.
  - An exact `look-at` would need a client-side hook in `Code/BF2`; this tooling does not change game
    code.

### The scenarios

| Scenario | Map / mode | Poses (world x/y/z) | Checks |
| --- | --- | --- | --- |
| `infantry-range` | dalian_plant, coop, 4 bots, `--ai-spawn-only` | player -80/154.3/136 (grass plateau on the lake's north shore, facing NE); enemy bot -67.5/154/157.65 (25 m ahead); friend bot -85.2/154.3/133 (6 m left) | aim at the enemy's chest -67.5/153.8/157.65 and fire 6 short re-aimed bursts: `[bot] enemy ... health` drops and `[kill] ... with usrif_m4`; the killed bot respawns and is placed back. Then look at the friend, walk -92/130 -> -104/128 -> -80/136 on open grass, suicide, respawn at the same pose and aim at the enemy again. |
| `render-view-dalian` | dalian_plant, conquest, no bots | player -80/154.3/136; `road` -89.6/152.0/157; `site` -167.5/154.2/-16.5; `entrance` -190/154.2/-250 | captures `view-horizon` (aim -67.5/155.5/157.65: hills, sea horizon, sky), `view-water` (-80/154.5/106: the lake, cooling towers), `view-road` (-100/150.5/157: dirt road through grass, terrain detail, storage hall), `view-shadows` (-212/153.2/-52: construction-site yard, crane and scaffold shadows on the dirt, tank), `view-vegetation` (-223/158/-248: birch trees, bushes, fences) |
| `render-view-ocs` | operation_clean_sweep, coop, 2 still bots | player -560/30.7/505 (beach south of the airfield); `sea` -605/30.7/552; `palms` -600/50.4/626 | captures `view-shore` (-560/29/440: beach, docks with boats, water, islands), `view-sun` (-560/60/420: low sun over the water), `view-sea` (-680/29/552: open water surface, far shore), `view-palms` (-560/29/450: palms above the bay) |
| `shadows-view` | dalian_plant, conquest, no bots, `--video-high` | player -167.5/154.2/-16.5 (construction-site yard) | the High-quality shadow check at a fixed view (aim -212/153.2/-52: terrain, scaffold and crane shadows, tank, hills): `view-pre`, `fire-01..03` during a burst, `after-01`, `fire-04`, `after-02`, `view-hill` (-212/165/-52). Measure the black fraction on rows 10-80 % (max channel <= 3). The first High run compiles effects for minutes (spawn at t=128 s in run-20261105-192908) |

Notes:

- On operation_clean_sweep, `--bots 0` left the spawn screen without its HUD (kit list, map), so no
  spawn happened. The same happened with the shorter Dalian menu waits (`run-20261105-185653`,
  `-190238`). `render-view-ocs` therefore uses 2 bots and its own `join-ocs.txt` with the original
  menu waits and the airfield spawn (`hud-click 466 138`).
- `join-and-spawn` (shared by all Dalian scripts) now waits about 14 s less. It waits for the spawn on
  `[PLAY] alive=1` instead of 10 s, and its menu waits are 3000/3000/1000/2000 ms instead of
  5000/4000/1500/3000.

Repeatability, two runs on the same build (rend-track build, mean |diff| 0-255 over RGB, leaving out the
pistol corner; the second figure after a 4x4 box downscale):

| View | full-res | 1/4-res | What remains |
| --- | --- | --- | --- |
| dalian `view-horizon` | 0.58 | 0.27 | |
| dalian `view-water` | 3.56 | 2.04 | water animation |
| dalian `view-road` | 2.55 | 1.12 | |
| dalian `view-shadows` | 3.66 | 1.87 | sub-pixel edge shifts of the scaffold and crane lattice (a residual view angle under one mouse count) |
| dalian `view-vegetation` | 5.16 | 2.43 | the birch and bushes sway in the wind |
| ocs `view-shore` | 0.21 | 0.16 | |
| ocs `view-sun` | 0.13 | 0.10 | |
| ocs `view-sea` | 3.47 | 3.00 | water animation |
| ocs `view-palms` | 6.01 | 3.43 | palms sway in the wind |

Runs: dalian `run-20261105-194641` / `-194808`; ocs `run-20261105-195114` / `-195245`.

### Comparing render views (`compare_views.py`)

```sh
L=tools/macos/loopback
$L/compare_views.py out/loopback/run-A out/loopback/run-B           # reference, candidate
$L/compare_views.py --baseline-save render-view-dalian-ok out/loopback/run-A   # -> out/baselines/<name>/
$L/compare_views.py out/baselines/render-view-dalian-ok out/loopback/run-B
```

- **Inputs.** The script compares every `view-*` capture present in both directories, a run or a saved
  baseline.
- **Mask.** It leaves out these regions (800x600):
  - the pistol corner (x >= 520 and y >= 340);
  - the bottom HUD bars (y >= 560);
  - the minimap (x >= 590 and y < 210);
  - the chat lines (x < 300 and 60 <= y < 160).
- **Output.** It prints the mean |diff| (0-255 over RGB) and the share of pixels whose largest channel
  difference exceeds `--pixel-threshold` (24).
- **Limits.** They come from `scenarios/<name>/limits.cfg`. The scenario is taken from
  `<run>/scenario.txt`, which `loopback.py --scenario` writes, or from `--scenario`.
- **Failures.** For each view over its limit, or missing on one side, it writes a reference | candidate
  | heatmap PNG to `<candidate>/compare/` (or `--out`) and exits 1.
- **Baselines** live under `out/` and are never committed.
- **Decoder.** PNG decoding uses zlib only, about 2.5 s per view pair on the i7.

Verified:

- The same-build pairs above pass, all views within their limits.
- The coordinator's before/after pair from the old single-step aim fails on every view, shadows
  worst (22.9 against a limit of 5.5; `rend-track/out/loopback/run-20261105-192235` vs `-192736`).
- `shadows-view` has no limits: it is a black-fraction check, not a pixel comparison.

### Picking positions by hand (`--mark-session`)

```sh
tools/macos/loopback/loopback.py --build-dir out/build/<tag> --mark-session --map dalian_plant --ai --ai-spawn-only --bots 4 --spawn-time 1
```

The client starts with no input script and runs for up to 1800 s. Add `--scenario NAME` to have that
scenario's bots placed. Join, spawn, then open chat with J (all) or K (team) and send `mark NAME`. The
server answers in chat and appends three things to `<run>/playtest_marks.txt` (copied from the server
root at the end, path printed at client start):

- a scenario line (`player x y z yaw`; use the same values in a `bot enemy|friend ...` line for a bot);
- an `aim X/Y/Z` point 15 m ahead at chest height;
- a `--pose` value.

`[mark]` lines also go to `playtest_pose.log`.

The yaw comes from `getRotation`, which is the server soldier's body yaw, not the exact view. In
`run-20261105-181901` it matched the client view within 1 degree once (31.2 vs 30) and was 10.5
degrees off once (100.5 vs 90). The pitch is always 0, so the suggested aim point is approximate.
