# Startup movies

[Issue #41](https://github.com/B2BF/OpenBF2/issues/41) tracks the client startup
movies that previously required `+restart 1` to bypass. The implementation belongs
to RendDX9's `GeomMisc/MenuRend/FSMoviePlayer.cpp`, with decoding and texture
ownership in `Video.cpp`. Both files already belong to the renderer build manifest.
The dedicated server does not play these movies.

## Recovered behavior

The original client queue already matches the reconstruction: create a movie player,
try its filename, release failed loads, draw successful loads, and advance when the
movie finishes or a permitted Escape skip occurs. The missing renderer path now:

- Loads the stock `mods/xpack/Shaders/LoadingScreen` effect and its `TRANSFORM`
  and `TEXTURE` parameters.
- Creates `dice.hfe.geom.Video` through the native host's by-value name overload,
  initializes Bink's DirectSound output, and opens the movie.
- Acquires its own reference to the VideoTarget. Video retains its separate
  reference, so releasing the movie player cannot free the target twice.
- Creates the retail four-vertex, 24-byte-stride quad and vertex declaration.
  Texture coordinates span zero to one, and the previous buffer pool is restored.
- Lets Video gate decoding on Bink's clock, sets the identity transform, binds the
  actual D3D texture, draws pass zero as a two-triangle fan, unbinds the texture,
  and presents. A failed scene begin still attempts presentation.
- Releases parameters, effect, frame target, Video, declaration and vertex buffer
  in retail order. Partial loads leave acquired resources for final destruction.

The Video and VideoTarget interface IDs are corrected to `0xB801DD04` and
`0xB801DD03`. Video's virtual frame getter now returns an owned reference, matching
retail. Internal menu texture readers retain borrowed access. No virtual slots,
network fields or content formats were added or reordered.

The original loader assumes its registered Video factory succeeds; the original
terminal cleanup leaves member pointers unchanged. This recovery preserves those
contracts rather than introducing a new failure policy or reusable cleanup method.

## Evidence

The original PE32 images were opened read-only in IDA and their instructions,
callers, interface checks and data constants inspected directly:

| Image | MD5 | Relevant bodies |
| --- | --- | --- |
| BF2.exe | `4c843dcee79388341f72c3a7adb1ace6` | `playMovie` `0x407A60`, `checkMovie` `0x407CB0`, movie draw/skip/advance in `mainLoop` `0x40CF0E` |
| RendDX9.dll | `aaf43741c3e1504f8d1498d366f9f4fa` | Movie load `0x100A1FE0`, cleanup `0x100A2200`, constructor `0x100A2290`, draw `0x100A2300`; Video frame acquisition `0x1015A930`, sound initialization `0x1015A8F0`, empty sound-shutdown slot `0x1015A910`, Video/target interface checks `0x1015AC80` / `0x1015A7F0` |

The declaration bytes at `0x1020D350` describe position FLOAT3 at offset zero,
packed color at offset twelve and texture FLOAT2 at offset sixteen. The movie draw
calls D3DX `SetTexture` directly; the generic shader parameter texture setter
expects a different engine texture wrapper and cannot be substituted here.
The native test's emitted MSVC assembly independently confirms the by-name factory
dispatch at vtable offset `0x2C`. Declaration-order comments for the overloaded
ClassManager methods are not an MSVC slot map.

These are client-only PE implementations. No Linux dedicated-server behavior was
changed or inferred from the renderer.

## Running and checking

Normal CMake/Visual Studio and checked-in client run arguments now omit `+restart`.
The switch remains available for an explicit intro skip. Its presence sets
`skipMovies`; changing its value to zero does not enable playback. Existing CMake
caches retain their old `BF2_CLIENT_RUN_ARGS` until changed or reset. Set that cache
entry to `+modPath;mods/bf2;+fullscreen;0;+multi;1` to enable normal startup.

Use a separate rebuilt client output with its matching native plugins and the stock
Bink/D3DX runtime dependencies. Preserve the original game binaries. The ordinary
startup sequence is EA, DICE, legal, intro and welcome, followed by the menu. The
first three are unskippable; intro and welcome permit Escape.

Validation completed for this change:

- The isolated native x86 RendDX9 build compiled all 131 translation units and
  passed strict linking and export/import checks.
- A fresh MSVC x86 CMake configure passed and generated client launch arguments
  without `+restart`. The check explicitly selected Visual Studio's Ninja because
  the unrelated `depot_tools` Ninja earlier on this machine's PATH failed to run.
- `Code/Tests/fsmovie_runtime.cmd`: 114 checks passed, strict x86 link without
  `/FORCE`. It exercises the production movie-player body and unchanged extracted
  ShaderParameter methods with recording collaborators. Checks include native
  string dispatch, failed loads, geometry, state/draw order and terminal cleanup.
- `Code/Tests/video_bink_d3d_runtime.cmd <fresh-output> <renderer-objects> <game-dir>`:
  118 checks passed on an AMD Radeon RX 6950 XT D3D9 HAL device. The production
  Video code opened all five stock movies, decoded twelve clock-gated frames each,
  and uploaded pixels matching Bink output exactly. It also checks frame ownership,
  pause/resume callbacks, missing-file cleanup and interface rejection. Its file
  paths and device/listener host are explicit test scaffolding.
- `Code/Tests/fsmovie_d3d_runtime.cmd <fresh-output> <renderer-objects> <game-dir>`:
  14 checks passed with the real movie, Video/Bink, Effect/FXParameter,
  ShaderManager, BufferManager, StateManager and renderer frame paths. The stock
  LoadingScreen shader rendered the decoded frame correctly: 5,292 sampled RGB
  channels had zero mismatches against CPU sampling of the decoded texture.
  The readback also received visual inspection. Device/base initialization,
  shader-cache seeding and host services are explicit component scaffolding;
  manager teardown is outside this fixture.
- An isolated copy of the existing client ran with `+modPath mods/bf2 +fullscreen 0 +multi 1`
  and no `+restart`. Its retained logs show all five startup movies opening in
  sequence and advancing into the menu. Live native PDB inspection confirmed no
  current/pending movies and an active menu; the owned window closed normally.
  This test used the existing October 2 client executable with the newly built
  renderer and matching staged native plugins, with Dev content. The first run's
  window was initially hidden; it was not evidence of visible or audible playback.
  A second launch used the approved visible window and also drained the movie queue
  into the menu. Window enumeration confirmed visibility without forcing it on.
  That run advanced from EA at 11:21:06 to the menu at 11:23:58, then exited with
  code zero after a normal window-close request.

The component fixtures alone do not establish complete client startup. Audible
output and a real device-loss/reset cycle were not verified. No server or client
network session was started. Builds and logs are isolated under `out/startup-movies-20261006`,
`out/issue41-fsmovie-fixture-20261006` and `out/issue41-fsmovie-gpu-20261006-02`.

## Remaining boundaries

The load, draw and quad-cleanup frontiers are removed together. Existing
`fsmovie-native-string-adapter` debt remains: the host's native by-value string is
converted to the movie player's internal VC7.1 path storage. Existing
`video-open-cwd-fallback` debt also remains for content roots separate from the
executable. Neither is a new compatibility mode. Video's existing device-loss
behavior and chosen open/failed-create guards were not redesigned by this change.
The production run still reported the pre-existing `ModuleGeomMisc::init` console
registration and `SwiffRender::beginDisplay` FontRenderer-begin diagnostics. Those
are outside movie playback and did not prevent this sequence reaching the menu.
