# Building the engine and tools

Run commands from the repository root unless a step says otherwise. The checked-in
[presets](../CMakePresets.json) select Ninja (Unix Makefiles on macOS) and a build
configuration; the Windows presets expect an initialized Visual Studio environment.
They do not switch an x64
compiler into x86 mode. In an IDE, select the matching Visual Studio architecture.

## Targets and outputs

GitHub Actions builds the supported Windows, Linux and macOS images on pushes and pull requests,
and attaches one package per platform when a release is published (`openbf2-windows.zip`,
`openbf2-linux.zip`, `openbf2-macos.dmg`). See [CI and release packages](ci-releases.md)
for the matrix, archive contents, checks and current runtime limitations.

Paths below are relative to the selected CMake build directory.

| Preset | Target | Output | Link contract |
| --- | --- | --- | --- |
| `msvc-x86` | `bf2_recovered` | Static engine archive | Compile check |
| `msvc-x86` | `bf2_w32ded` | `bin/bf2_w32ded.exe` | Strict |
| `msvc-x64` | `bf2_w64ded` | `bin_w64ded/bf2_w64ded.exe` | Strict |
| `linux-x64` | `bf2` | `bin_linux/bf2` | Strict |
| `macos-x64` | `bf2` | `bin_macos/bf2` | Strict |
| `macos-universal` | `bf2_client`, `bf2`, `bf2_aidll`, `dice_py`, `dice_py_check`, `bf2_aidll_loader_check` | `bin_macos/` | Strict; arm64 + x86_64 slices; native effect dependencies required for the renderer |
| `linux-x64` / `macos-x64` | `bf2_aidll` | `bin_linux/libaidll.so` / `bin_macos/libaidll.dylib` | Strict; native AI image |
| `macos-x64` | `bf2_client` | `bin_macos/bf2_client` | Strict; renderer excluded; renderer-free startup reaches the main loop (S2.12) |
| `msvc-x86` | `bf2_client` | `bin_client/BF2.exe` | Strict |
| `msvc-x86` | `bf2_editor` | `bin_editor/BF2Editor.exe` | Strict; voice dependency fix awaits VM verification |
| `msvc-x64` | `bf2_client` | `bin_client/BF2.exe` | Strict |
| `msvc-x64` | `bf2_aidll` | `bin_client/aidll.dll`; staged into `bin_w64ded/` | Strict; native AI image, pending MSVC validation |
| `msvc-x64` | `bf2_editor` | `bin_editor/BF2Editor.exe` | Strict; voice dependency fix awaits VM verification |

The Windows release package, `openbf2-windows.zip`, ships both Windows clients (`x64\`,
`x86\`), the x86 dedicated server (`server\`) and the x86 editor (`editor\`). An x64 editor
package is a future goal.

The x86 preset uses `Reconstruction` with `BUILD_TESTING=ON`. Windows x64 uses `Release`;
Linux uses `RelWithDebInfo`; both default to `BUILD_TESTING=OFF`. The client and editor
are explicit targets on Windows x86 and x64 and need the
[April 2005 DirectX SDK](#directx-sdk). The optional `bf2_aidll` target
builds `bin/AIDLL_w32ded.dll` with a diagnostic link in the x86 configuration.
On Windows x64, both `bf2_client` and `bf2_w64ded` depend on the strictly linked
native `bf2_aidll`; the server stages the same `aidll.dll` beside its executable.
This is `[CHOSEN, not recovered]` under [issue #11](https://github.com/kr4uzi/OpenBF2/issues/11).

On POSIX, building `bf2` also builds `bf2_aidll` from the same native headers and
toolchain. Keep the resulting library beside the executable; never use the retail
32-bit `libaidll.so`. The AI still contains named aborting frontiers. The optional
`bf2_aidll_loader_check` target checks dynamic loading and export lookup without
initializing AI or starting a server.

A strict link resolves the image's symbols. Runtime behavior still needs validation.
A `/FORCE` link can produce an executable with unresolved calls, so treat it as a
diagnostic artifact and report the exact paths a run exercised.

## Windows prerequisites

- MSVC C++ tools and Windows SDK, with x86 and x64 components as needed.
- For the client, the editor and their renderer plugins: the April 2005 DirectX SDK
  (see [DirectX SDK](#directx-sdk)). Dedicated servers do not need it.
- CMake 3.21 or newer and Ninja on `PATH`.
- Your own Battlefield 2 1.50 installation for runtime content and reference binaries.
  Compiling an archive or running engine-free checks does not require game content.

The C++ build uses C++14 and MSVC permissive mode. Python 2.3.4 is built from the vendored
source into `dice_py`; a system Python installation does not replace it.

The batch helpers enter the MSVC environment through `Code\Build\vsdevcmd.cmd`, which
resolves `VsDevCmd.bat` from the `BF2_VSDEVCMD` environment variable (its full path) or,
when that is unset, a `vswhere` auto-detected installation carrying the x86/x64 VC tools.
Set `BF2_VSDEVCMD` once per machine instead of editing scripts; if neither resolves, the
helper reports it and stops. CMake compilation uses the toolchain already present in your
terminal.

## Windows x86

Open an x86 Visual Studio developer terminal. Build the archive and focused checks first:

```powershell
cmake --preset msvc-x86 -B out/build/local-x86
cmake --build out/build/local-x86 --target bf2_recovered bf2_run_checks
```

For the dedicated executable, build and check the matching Python runtime, then configure
again so CMake sees the DLL and installs its staging rule:

```powershell
cmd /c Code\Build\python\build_python.cmd x86
cmake --preset msvc-x86 -B out/build/local-x86 -DBF2_GAME_DIR="D:/Games/Battlefield 2"
cmake --build out/build/local-x86 --target bf2_w32ded
```

Replace the example game path with your installation. The default Python outputs are
`Code/BF2/External/python-embedded/OBJ/python/release/dice_py.lib` and `dice_py.dll`.
CMake stages the DLL beside the executable.

## Windows x64

Use a separate **x64** Visual Studio developer terminal and build directory:

```powershell
cmd /c Code\Build\python\build_python.cmd x64
cmake --preset msvc-x64 -B out/build/local-x64 -DBF2_GAME_DIR="D:/Games/Battlefield 2"
cmake --build out/build/local-x64 --target bf2_w64ded
```

The x64 Python pair defaults to
`Code/BF2/External/python-embedded/OBJ/python/x64/release/`. Never reuse the x86 import
library or DLL. This target is a native dedicated-server port; its object layouts do
not validate retail x86 offsets. The same configuration also builds the native x64 client
and editor (see [Client and plugins](#client-and-plugins)).

For isolated Python builds, set `BF2_PYTHON_OUT` before calling `build_python.cmd`, then
configure CMake with `-DBF2_DICE_PY_LIB=<output>/dice_py.lib` and
`-DBF2_DICE_PY_DLL=<output>/dice_py.dll`. Use an architecture-specific absolute output
path and remove the environment override after the build. The helper defaults to x86
when no architecture argument is supplied and also runs its interpreter check.

## Linux and WSL

Use Linux x86-64 with CMake 3.21 or newer, Ninja, GCC or Clang, and ncurses development
headers (`libncurses-dev` on Debian). From the repository root:

```bash
cmake --preset linux-x64 -B ~/bf2-linux-local -DBF2_GAME_DIR="/path/to/Battlefield 2"
cmake --build ~/bf2-linux-local --target bf2 dice_py_check_run
```

Under WSL, keep the build directory in the Linux filesystem. The Linux-only preset is
available when CMake itself runs on Linux. CMake builds `libdice_py.so` beside `bf2`;
the executable uses an `$ORIGIN` runtime search path to find it. `dice_py_check` builds
the check executable; `dice_py_check_run` builds and runs it.

`BF2_LINUX_STATIC_RUNTIME` defaults to `ON`, linking libstdc++ and libgcc statically.
glibc and the other system dependencies remain dynamic. Pass
`-DBF2_LINUX_STATIC_RUNTIME=OFF` if you need dynamic compiler runtimes instead.

The client-only `bf2_gamespy_voice_objects` target is excluded from the default build;
Linux has no Voice2 hardware backend. Windows/macOS client and voice-test dependencies
still request it explicitly.

## macOS

The macOS dedicated server is a native port of the Linux one: the same `bf2` sources
and `[CHOSEN, not recovered]` 64-bit layouts, built with AppleClang and libc++. It is
an owner-requested port change; see [the macOS port notes](macos-port.md) for what
differs from Linux and what the client and Metal milestones still need.

Prerequisites are the Xcode Command Line Tools (Apple clang, the macOS SDK and
`make`) and CMake 3.21 or newer. Full Xcode and Ninja are not required. The SDK
supplies ncurses, pthreads and `dlopen`.

The macOS configuration also defines the client and requires its native effect
dependencies ([CHOSEN, not recovered], owner decision 2026-11-01). With an existing
x86_64 native dependency bundle, configure and build the renderer-ON client and server
in one command from the repository root (`NATIVEFX_DIR` is the bundle's directory, built
with `sh tools/macos/nativefx/build.sh`):

```bash
cmake --preset macos-x64 -B out/build/local-macos -DBF2_MACOS_LINK_RENDDX9=ON -DBF2_FX_NATIVE_DEPS="$NATIVEFX_DIR" && cmake --build out/build/local-macos --target bf2_client bf2 -j4
```

`BF2_FX_NATIVE_DEPS` names a directory with `vkd3d/.libs/libvkd3d-shader.a`,
`vkd3d/.libs/libvkd3d-common.a`, `vkd3d-src/include/vkd3d_shader.h`,
`mojo/libmojoshader.a` and `mojo-src/mojoshader.h`. The libraries must contain every
requested architecture and use the repository's vkd3d/MojoShader patches. CMake
checks all three archives with `lipo -verify_arch`. The default directory is
`out/build/nativefx` for x86_64, `out/build/nativefx-arm64` for arm64, or
`out/build/nativefx-universal` for both; if dependencies are absent or incompatible,
renderer-enabled configuration fails with instructions instead
of building a cache-only client. Local dependency sources are not vendored in this
checkout. If already supplied under `out/third_party/vkd3d` (vkd3d 2.1) and
`out/third_party/mojoshader`, `sh tools/macos/nativefx/build.sh` builds the dependencies
offline there. That optional source build also needs the native autotools/libtool,
flex, bison and MinGW widl tools expected by `build-vkd3d.sh`; nothing is downloaded
or installed. The production CMake build consumes the native libraries directly and
does not invoke Wine, D3DX or an oracle tool. Pass `arm64` or `universal` to
`build.sh` to cross-compile or assemble both slices with `lipo`; no argument keeps
the historical x86_64 output. All use the same patched sources and default to
`MACOSX_DEPLOYMENT_TARGET=11.0`. Cross builds use host-native generators and never
execute the arm64 harness.

The client compiles effects with vkd3d-shader, the D3D9 asm assembler, the effect
parser and MojoShader to MSL, caching under `@HOME@/FxNative`. No Wine, Windows DLL,
offline prebuild or identity file is needed to install or run it. See the
[effect policy](macos-port.md).
Optional historical oracle/cache checks require `-DBF2_FX_DEVELOPER_ORACLE=ON`;
they are excluded from the default build and are never client dependencies.

With native dependencies in the default directory, the server command remains:

```bash
cmake --preset macos-x64
cmake --build out/build/macos-x64 --target bf2 dice_py_check_run -j4
```

The preset uses the Unix Makefiles generator, `RelWithDebInfo`, and
`CMAKE_OSX_SYSROOT=macosx`. The explicit SDK sysroot keeps Homebrew headers in
`/usr/local/include` out of the build. The outputs are
`out/build/macos-x64/bin_macos/bf2`, `libdice_py.dylib` and `libaidll.dylib`;
`bf2` finds the libraries through an `@loader_path` runtime search path, so copy
the three together. `+ai 1` enables the AI connection. A co-op mode also needs a
map with its matching AI layer and navigation files.

`macos-x64` explicitly selects `CMAKE_OSX_ARCHITECTURES=x86_64`, including on Apple
silicon. The `[CHOSEN, not recovered]` `macos-universal` preset selects
`CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` for the server, client and plugins.
With the local sources and generator tools above available, this single command
builds the native dependencies, Metal client, dedicated server and AI library:

```bash
sh tools/macos/nativefx/build.sh universal && cmake --preset macos-universal -B out/build/local-universal -DBF2_MACOS_LINK_RENDDX9=ON && cmake --build out/build/local-universal --target bf2_client bf2 bf2_aidll -j4
lipo -archs out/build/local-universal/bin_macos/bf2
```

Python's Darwin configuration selects `va_list` handling per compiler slice; the
other configured widths are shared. No host Python or additional software is needed.
For an arm64-only compile measurement, use a separate directory:

```bash
sh tools/macos/nativefx/build.sh arm64 && cmake --preset macos-x64 -B out/build/local-arm64 -DCMAKE_OSX_ARCHITECTURES=arm64 -DBF2_MACOS_LINK_RENDDX9=ON && cmake --build out/build/local-arm64 --target bf2_client bf2 bf2_aidll -j4
```

Cross-compiling and linking arm64 does not establish native runtime behavior. See
[stage 1a](macos-port.md#stage-1a-universal-build-and-m2-validation) for the owner-run
M2 bundle, offline sandbox, port 16890 and remaining client blockers.
These build commands do not run a client
or server. On an Intel host the arm64 vtable probes compile but their executable
checks are left for the M2; universal executable checks use the host slice.

`bf2_client` is the macOS client target ([CHOSEN, not recovered], package S2.0). Its
renderer, movie and audio plugins are static libraries linked into the executable. The
Metal renderer is enabled with `BF2_MACOS_LINK_RENDDX9=ON`; OFF retains the earlier
renderer-free diagnostic client. Native effect dependencies are required at configure time
when the renderer is enabled. The
renderer-excluded x86_64 client compiles and links strictly after the
PR #55 integration (`--target bf2_client -j4`). Renderer-free
startup reaches the main loop; see [S2.12](macos-port.md#renderer-free-startup-end-s212).
Run it only through the sandboxed offline harness, with HOME and cloned content under
`out/`. Build `bf2_renddx9_static` separately with `-- -k` to measure the remaining
renderer compile blockers.

Run the server from a content root as on Linux. Pass `+config` and `+mapList` paths
**relative to the content root**: with an absolute `+config` path the macOS smoke run
ignored the file and started with the default settings, including port 16567. Use
`+noStatusMonitor 1` when no terminal is attached. The ncurses status monitor is the
only console input, so a graceful `quit` needs a terminal; the server installs no signal
handler, and `SIGTERM` ends it without the shutdown path.

### macOS client options that are not retail

These `[CHOSEN, not recovered]` port additions exist only in the macOS client:

| Option | Effect |
|---|---|
| `+help 0` / `+help 1` | Hides / shows the tutorial messages for this session (option id 57; bare `+help` keeps the retail usage text). |
| `+warmupLevel <level>` | Hosts `<level>` locally through the non-menu start path, logs in the `+playerName` profile (else the default user) so its video settings apply, builds every shader of the level load, presses JOIN GAME through the button's own action (`dice.bf2.Logic.readyToPlay`) so the first world frame builds its effects too, waits for the background effect builds and exits. Runs windowed whatever `+fullscreen` says (a fullscreen client started behind another app is deactivated, and the deactivated main loop draws neither the menu nor the world). Without a logged-in profile the menu that loads after JOIN GAME asks to log out (renderMenu type 13), which would end the hosted game; the warm-up drops the game-ending menu events (8, 13, 14, 17) and prints `warmup: menu event N dropped`. `warmup: effects N/M` is printed at most once a second whenever N or M changes, during the level load too. Option id 74 (ids up to 73 are retail). For the setup assistant: one run per shader set fills the native effect cache. stdout lines `warmup: start`, `warmup: profile`, `warmup: effects N/M`, `warmup: level loaded`, `warmup: player joined`, `warmup: join game`, `warmup: world frame`, `warmup: done ...`; exit status 0 done, 2 the start failed, 3 timeout (`BF2_WARMUP_TIMEOUT`, default 3600 s), 4 done with native build failures. Check: `tools/macos/rendergate/warmup.sh`. |

## Client and plugins

The optional vanilla widescreen HUD is enabled with `+widescreenHud 1` (default
off). The renderer's wider FOV cap and water-buffer guard apply regardless of this
option. See [widescreen HUD](widescreen-hud.md) for attribution, scope and validation.

Incoming VoIP packets with truncated trailers, empty payloads, or partial codec frames
are rejected before playback or sender notification. This is intentional hardening
(`[CHOSEN, not recovered]`, [issue #26](https://github.com/kr4uzi/OpenBF2/issues/26));
valid wire formats and sender/mute handling are unchanged. The macOS
`bf2_voip_client_check` target covers malformed lengths, valid frames and loopback UDP
using synthetic audio and game objects. It does not establish real peer audio or gameplay.

Use the x86 or x64 configuration and the matching Python runtime above:

```powershell
cmake --build out/build/local-x86 --target bf2_client
cmake --build out/build/local-x86 --target bf2_client_linkcheck
```

The x64 configuration builds the same targets (`out/build/local-x64`). See
[Native x64 client](#native-x64-client) for what differs on x64.

`bf2_client` rebuilds and stages `RendDX9.dll`, `SwiffPlayer.dll`, `BF2Audio.dll`, and
`TextureAtlasBuilder.dll` beside `BF2.exe`. On x86 it also builds `BF2VoipServer.dll`
there. The client links strictly on both Windows architectures.
`bf2_client_linkcheck` and the x86 server's `bf2_w32ded_linkcheck` are buildable
compatibility aliases for their production targets; they emit no separate executable.
The x86 voice and AI DLLs retain independent diagnostic `/FORCE` links.

The client plugin steps currently share `Code/Build/obj_*_native` and
`Code/Build/bin_*_native` (x86) or `Code/Build/obj_*_x64` and `Code/Build/bin_*_x64` (x64)
across CMake trees of that architecture. **Serialize client builds.** The staging
paths are fixed, so redirecting a plugin's `BIN` environment variable during a CMake
client build would make it stage from the wrong location.

Standalone plugin scripts build for the rebuilt client. No argument and the optional
`native` argument select the x86 build; `x64` selects the native x64 build; other
arguments are rejected.
Renderer, movie, and audio DLLs share the host's dynamic CRT and require
`_ITERATOR_DEBUG_LEVEL=0`. Retail executable/DLL swapping is retired under
[issue #7](https://github.com/B2BF/OpenBF2/issues/7). Gameplay and content compatibility
remain requirements.

For example, from PowerShell:

```powershell
cmd /c Code\Build\renddx9.cmd
cmd /c Code\Build\swiffplayer.cmd
cmd /c Code\Build\bf2audio.cmd
cmd /c Code\Build\textureatlasbuilder.cmd
cmd /c Code\Build\renddx9.cmd x64
```

Default DLL directories remain `Code/Build/bin_renddx9_native`,
`bin_swiffplayer_native`, `bin_bf2audio_native`, and `bin_textureatlasbuilder_native`;
the `x64` argument uses the `_x64` suffix instead of `_native`.
Object directories use `obj_` instead of `bin_`. Existing compatibility outputs are
not selected or removed by these scripts. Standalone builds support these isolation overrides:

| Script | Object directory | DLL directory |
| --- | --- | --- |
| `renddx9.cmd` | `BF2_RENDDX9_OUT` | `BF2_RENDDX9_BIN` |
| `swiffplayer.cmd` | `BF2_SWIFFPLAYER_OUT` | `BF2_SWIFFPLAYER_BIN` |
| `bf2audio.cmd` | `BF2_BF2AUDIO_OUT` | `BF2_BF2AUDIO_BIN` |
| `textureatlasbuilder.cmd` | `BF2_TEXTUREATLAS_OUT` | `BF2_TEXTUREATLAS_BIN` |
| `memory.cmd` | `BF2_MEMORY_OUT` | `BF2_MEMORY_BIN` |

TextureAtlasBuilder uses its own static CRT and legacy float rounding to preserve `.tai`
output. It does not transfer ownership of CRT allocations across the DLL boundary;
callback string references are borrowed. The rebuilt plugins
do not depend on `Memory.dll`, `msvcr71.dll`, or `msvcp71.dll`.
`memory.cmd` builds the recovered `Memory.dll` (`Code/BF2/Memory`) into
`bin_memory_native` (`bin_memory_x64` for x64); no game image links it.
The renderer and atlas DLLs compile and link against the installed DirectX SDK described
below, and at run time need the matching `d3dx9_25.dll`. Client runtime dependencies also
include the game's Bink component, TextureAtlasBuilder, and an OpenAL provider for audio.
The audio build always compiles the vendored Ogg/Vorbis decoder in, as the shipped DLL links its own.

The CMake client target stages all four rebuilt DLLs above. It also stages Python
when present at configuration time. Keep runtime testing in a separate
directory and preserve the original images as references. Original-binary differential
fixtures are isolated validation tools, not a supported retail plugin build.

Native-capable renderer and plugin fixtures now select only the rebuilt ABI. The old
`light_manager_runtime.cmd`, `light_manager_shadowmaps_runtime.cmd`,
`swiffplayer_host/build.cmd`, and `bf2audio_diff.cmd` entrypoints return an explicit
retirement diagnostic because their observers assume a retail host. Their fixture
sources remain available for a future native port. Use native focused fixtures,
`renddx9_host/build.cmd`, `swiff_disconnect_runtime.cmd`, and `bf2audio_smoke.cmd`
for the corresponding rebuilt boundaries; none establishes complete gameplay coverage.
The renderer registration host currently reaches device-dependent LightManager
construction without supplying a device. That fixture crashes with both the pre-removal
and native-only DLLs; it is not a passing startup acceptance gate. Restore its required
host services before using it to claim plugin initialization coverage.

### Native x64 client

`[CHOSEN, not recovered]` under [issue #7](https://github.com/B2BF/OpenBF2/issues/7). The x86
build remains the retail-fidelity reference; the x64 images are native ports with their own
pointer-sized layouts. They keep the shipped file names, are written only inside the x64
build tree, and can be mixed only with each other.

- **PunkBuster.** Not supported. PunkBuster exists only as 32-bit DLLs (`pbcl.dll`,
  `pbsv.dll`), and an x64 process cannot load them, so the x64 client takes the
  "PunkBuster Client Not Installed" path. It **cannot join servers that require PunkBuster**;
  use the x86 client for those.
- **Movies.** `binkw32.dll` exists only as a 32-bit DLL, so the x64 `RendDX9.dll` has no Bink
  import. Each movie is skipped with one log line per movie name (`Video::open(...): movie
  skipped`), through the same failure return the x86 renderer uses when a `.bik` is missing.
- **Novint Falcon.** Archived on every platform (`Code/Archive/NovintFalcon`,
  [issue #17](https://github.com/kr4uzi/OpenBF2/issues/17)): no image imports or stages
  `NovintHFX.dll`, and the game takes retail's no-Falcon path.
- **Audio.** The default providers load `BF2OpenAL.dll`. The game directory's copy is 32-bit and
  an x64 process cannot load it. The x64 client (`x64\` in `openbf2-windows.zip`) carries
  OpenAL Soft built as a 64-bit `BF2OpenAL.dll` beside `BF2.exe`, and the application
  directory is searched first
  ([OpenAL Soft in the Windows clients](ci-releases.md#openal-soft-in-the-windows-clients),
  `Code/Build/build_openal_soft.py`). A local build without that DLL has no sound, with a
  `Could not find BF2OpenAL.dll` warning. Either run the script or select the **System OpenAL**
  provider with a 64-bit `OpenAL32.dll` installed.
- **D3DX.** The x64 renderer and atlas DLLs import the 64-bit `d3dx9_25.dll`, which the DirectX
  End-User Runtime installs in `System32`. The copy in the game directory is 32-bit.
  `[CHOSEN, not recovered]` On Windows (x86 and x64), both DLLs delay-load it (`/DELAYLOAD`
  in `renddx9.cmd` and `textureatlasbuilder.cmd`). `BF2Render::init` loads it before the
  window and the renderer. If it is missing, the client shows one message box ("The DirectX 9
  runtime (d3dx9_25.dll, 64-bit) is missing; run the OpenBF2 installer or the Microsoft
  DirectX End-User Runtime") and exits with code 1, instead of failing to load
  `RendDX9.dll` with a system error.
- **AI.** The native `aidll.dll` builds with the client and dedicated server and is packaged
  with both. It shares the host's native ABI; retail x86 AI DLLs cannot be used.
  Windows compilation, exports, strict linking and bot execution await coordinator validation.
- **Floating point.** Recovered x87 inline assembly keeps its x86 body; the x64 arms use SSE2
  with the same operation order, rounding and NaN handling, marked
  `[PLATFORM] x64: SSE2 arithmetic`. They match x87 at the CRT's 53-bit precision, not at the
  24-bit precision Direct3D selects on a device thread without `D3DCREATE_FPU_PRESERVE`.

Every x64 compile treats pointer truncation (C4311, C4312) as an error. The client links
strictly; `bf2_client_linkcheck` builds the same target.

**High-DPI displays (x86 and x64).** Retail `BF2.exe` has no manifest and does not declare DPI
awareness, and the rebuilt one does not either (owner decision 2026-11-06: behave like shipped retail).
Windows therefore bitmap-scales the game window on displays above 100 % scaling, for retail and
rebuilt alike. A retail install that looks unscaled carries the compatibility setting *Override high
DPI scaling behaviour: Application*, stored as `~ HIGHDPIAWARE` under
`HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers` for its path. Set the same
layer for the rebuilt `BF2.exe` path for a 1:1 window; the setting is per executable path, so every
staged copy needs its own entry. For pixel comparisons, give every compared client the same setting.

### DirectX SDK

`RendDX9.dll` and `TextureAtlasBuilder.dll` use the D3DX9 headers and import libraries of
the installed **Microsoft DirectX 9.0 SDK (April 2005)**, `D3DX_SDK_VERSION` 25. That
SDK's `d3dx9.lib` binds `d3dx9_25.dll`, the D3DX both shipped DLLs import, so the rebuilt
renderer gets the same effect compiler and texture loaders as retail. Its `DxErr9.lib`
supplies the atlas DLL's `DXGetErrorString9A`. A later SDK binds a later `d3dx9_NN.dll`
with a different shader compiler; the June 2010 SDK, for example, binds `d3dx9_43.dll`
and rejects `ps_1_x` targets by default. The build refuses such SDKs instead of linking
them. Without the April 2005 SDK the build can use the
[D3DX fallback](#d3dx-without-the-directx-sdk) instead.

`Code\Build\dxsdk.cmd` locates the SDK for every script that needs it: `renddx9.cmd`,
`textureatlasbuilder.cmd` (and so the `bf2_client` and `bf2_editor` plugin steps), the
renderer fixtures under `Code\Tests`, and the renderer section of `syntaxcheck.cmd`. It
checks `DXSDK_DIR`, which the SDK installer sets, and then the SDK's default folder under
`Program Files (x86)`. It accepts a directory only when `Include\d3dx9core.h` declares
version 25 and `Lib\<arch>` holds `d3dx9.lib` and `DxErr9.lib`. Run it after
`vsdevcmd.cmd`, because it *appends* the SDK's `Include` and `Lib\<arch>` directories to
`INCLUDE` and `LIB`. The Windows SDK therefore still provides `d3d9.h`, `dinput.h`,
`dsound.h`, `strsafe.h`, `d3d9.lib`, `dinput8.lib` and `dxguid.lib`; only the D3DX headers
and libraries come from the DirectX SDK. It also sets `BF2_DXSDK_DIR` and `BF2_DXSDK_LIB`.
Without a suitable SDK it names what is missing and fails, unless the caller supports the
fallback below.

To check an installation, run the helper by itself. It prints the SDK it selected, or
the reason it rejected each candidate:

```powershell
cmd /c Code\Build\dxsdk.cmd x86
```

The architecture argument selects `Lib\x86` or `Lib\x64`; the April 2005 SDK ships both,
and its x64 `d3dx9.lib` binds the 64-bit `d3dx9_25.dll`. Renderer sources take D3DX
through `Code/RendDX9Abi/D3dx9Sdk.h` and use the SDK's typed interfaces (`ID3DXEffect`
and the others), not hand-declared prototypes or fixed vtable offsets. The x64 client,
renderer and editor link against `Lib\x64`.
Include `D3dx9Sdk.h` only inside a `BF2_IMAGE_RENDDX9` or
`BF2_IMAGE_TEXTUREATLASBUILDER` region: the engine images compile every `Code/BF2`
source without a DirectX SDK.

### D3DX without the DirectX SDK

`[CHOSEN, not recovered]`. Microsoft no longer distributes the April 2005 SDK, so
`renddx9.cmd`, `textureatlasbuilder.cmd` and the renderer section of `syntaxcheck.cmd` can
build without it. The fallback changes only where the declarations and the import library
come from. The rebuilt DLLs still import the same `d3dx9_25.dll` by the same names, so the
effect compiler and texture loaders are retail's. It works for x86 and x64.

`BF2_D3DX` selects the source. These scripts call `dxsdk.cmd <arch> fallback`:

| `BF2_D3DX` | Behaviour |
| --- | --- |
| `auto` (default) | Use the April 2005 SDK when `dxsdk.cmd` finds it, otherwise the fallback. |
| `sdk` | Use the SDK only, and fail without it. |
| `def` | Use the fallback even when the SDK is installed. |

Callers without the `fallback` argument (the renderer fixtures under `Code\Tests` and the
G2w seam build, `BF2_GFX_SEAM=ON`) still require the SDK. `dxsdk.cmd` prints which source it
selected.

With the fallback:

- **Import library.** `d3dx9_25.lib` is generated with `lib /def /machine:<arch>` from
  `Code/RendDX9Abi/D3dx9_25.def`. The `.def` lists the 37 names the rebuilt images call,
  without ordinals. The x86 and x64 DLLs export the same 323 names, but four names have
  different ordinals on x64. On x86 the D3DX functions are `__stdcall`, so callers reference
  `_Name@bytes` while the DLL exports `Name`. `lib.exe` cannot produce that pairing, so
  `Code/RendDX9Abi/D3dx9Fallback.h` maps each decorated name onto the library's plain one with
  `/alternatename`. If a byte count is wrong, the link fails.
- **Declarations.** `<d3dx9.h>` resolves to `Code/RendDX9Abi/D3dxFallbackInclude/d3dx9.h`. That
  header places the renderer's D3DX subset (`Code/BF2/Platform/Gfx/D3DX9Subset.h`, shared with
  the macOS build) on top of the Windows SDK's `<d3d9.h>`. `D3dx9Sdk.h` is unchanged: the subset
  declares `D3DX_SDK_VERSION` 25.
- **Error text.** `<dxerr9.h>` resolves to the fallback directory too. The SDK's `DxErr9.lib`
  supplied `DXGetErrorDescription9A` (renderer effect errors) and `DXGetErrorString9A` (atlas
  builder errors). Without it, `Code/RendDX9Abi/DxErr9StandIn.cpp` supplies both. They return
  `HRESULT 0x........` instead of the name or description. `renddx9.cmd` links with
  `/NODEFAULTLIB:dxerr9.lib` because `Effect.cpp` requests that library by `#pragma`. Only the
  diagnostic text differs.
- **Checks.** `renddx9.cmd` fails when a `d3dx9_25.dll` import name carries an `@bytes`
  decoration. Such an image links cleanly but fails to load with `ERROR_PROC_NOT_FOUND`.

The subset's COM interfaces must list their methods in the slot order of the real
`d3dx9_25.dll`. `tools/d3dx/check_d3dx9_25.py` checks this against both architectures. It
needs the x86 and x64 `d3dx9_25.dll` (SysWOW64 and System32 of a system with the DirectX
End-User Runtime) and Microsoft's public symbols for each, all copied under `out/`. Its
`--pdb-urls` option prints the symbol-server URLs.

| Check | Evidence |
| --- | --- |
| Interfaces D3DX implements | `ID3DXBuffer`, `ID3DXEffectPool`, `ID3DXBaseEffect`, `ID3DXEffect`, `ID3DXEffectCompiler`, `ID3DXBaseMesh`, `ID3DXMesh`, `ID3DXRenderToSurface` and `ID3DXConstantTable` (declared in `D3dx9Fallback.h`, Windows only) match their implementation classes' vtables slot by slot, by the method names in the public symbols. Identical-code folding gives some stub slots another function's name. The `ID3DXBaseEffect` slots are therefore accepted when either `CEffect` or `CEffectCompiler` names them. |
| Interfaces the engine implements | `ID3DXEffectStateManager` (21 slots) matches the slots that `CLoggableDevice`'s setters call. `ID3DXInclude` matches `Open` at slot 0 and `Close` at slot 1, as `CPPInclude` calls them. |
| Exports | Every `.def` name is exported by both DLLs. Every `d3dx9_25` import of the retail `RendDX9.dll` (49) and `TextureAtlasBuilder.dll` (5) is exported by both DLLs (`--retail <game dir>`). |

Run it after any change to the subset or the `.def`:

```bash
python3 tools/d3dx/check_d3dx9_25.py --dir out/win64/d3dx --retail "<Battlefield 2 dir>"
```

Some interfaces have trailing methods that the subset does not declare: `ID3DXEffect`'s
`GetStateManager`, parameter blocks and `CloneEffect`, `ID3DXMesh::SetAttributeTable`, and
`ID3DXEffectCompiler::CompileShader`. No rebuilt code calls them. Declare them, in DLL order,
before using them.

## Editor

The editor uses the client engine of its configuration (x86 or x64) and its own
application entry point. Dear ImGui and ImGuizmo are vendored. Build its engine-free checks
before the application:

```powershell
cmake --build out/build/local-x86 --target bf2_editor_run_checks
cmake --build out/build/local-x86 --target bf2_editor
cmake --build out/build/local-x86 --target bf2_editor_linkcheck
```

`bf2_editor_run_checks` exercises framework code without loading the engine or game
content. Both editor targets reject unresolved image symbols. They share the client
engine's voice-server dependency: the rebuilt `BF2VoipServer.dll` on x86 (staged beside
`BF2Editor.exe`), or the client's native voice objects on x64. The dependency fix and
strict editor links await fresh VM verification; no retail editor
voice behavior is claimed. The x86 voice DLL retains its own diagnostic link.
Configuration reports each editor target's voice provider and fails if it is absent.
`bf2_editor_linkcheck` is a separate executable, independent of `bf2_editor`;
requesting `bf2_editor` builds `BF2Editor.exe` and its renderer staging dependency.
Use a new build directory when checking a different checkout, and retain verbose
link commands and Ninja response files when investigating unresolved symbols.

Each editor build normally rebuilds its native renderer and TextureAtlasBuilder under
`<build>/editor_renddx9/` and stages them into `<build>/bin_editor/`. Python is also staged. These renderer outputs are separate from
the client's shared plugin directories. Leave `BF2_EDITOR_KEEP_RENDDX9` unset for a
fresh build; setting it to `1` can retain previously built renderer and atlas DLLs.
The editor currently runs with sound disabled and does not load SwiffPlayer.

For a manual editor launch, use the game installation as the working directory and the
rebuilt executable's absolute path. The editor accepts `+loadMod bf2` and
`+loadLevel <level>`; its own `+help` lists supported switches. It does not use the
client's `+modPath` switch. See [editor architecture](architecture.md#the-editor) and
[console commands](editor-console.md) before scripting edits. Play-testing starts a
local server and requires the same private-port setup as other server tests.

## Runtime configuration and checks

`BF2_GAME_DIR` is a CMake cache path used as the run targets' working directory. Its
checked-in default points to a developer's installation; set it explicitly. Game
scripts, archives, and Python modules are resolved from that content root. Keep rebuilt
executables and their matching Python runtime in their build output directories.

Before using `bf2_w32ded_run`, `bf2_w64ded_run`, or `bf2_run`, prepare a private server
settings file and map list. `BF2_RUN_ARGS` takes a quoted CMake semicolon list, for
example `+modPath;mods/bf2;+config;<settings-file>;+mapList;<map-list-file>`. Initial
tests can use loopback and stock content to isolate startup and loading. Choose the game
mode and AI settings for the scenario being tested. Set an unused game port explicitly;
**16567 is reserved for interactive testing**. Keep settings, harness
`BF2_PORT`, and any test-client destination consistent. Follow the
[development validation rules](development.md#validation) before connecting clients.

The production executable runs until normal shutdown. `BF2_FRAMES` and other harness
controls belong to `Code/Build/bootmain.cpp` and do not limit production run targets.
`bf2_client_run` uses `BF2_GAME_DIR` and the `BF2_CLIENT_RUN_ARGS` semicolon list.

### Online services

Every image forms the GameSpy service names as retail does, with `gamespy.com` as the
domain. Those services no longer exist, so an unconfigured client or server finds no online
service. When the names do not resolve, logins report the connection failure, the internet
server list stays empty, LAN play is unaffected, and a server with `sv.internet 1` stops at
start-up with the fatal "cannot communicate with the required internet backends"
diagnostic.

`+gamespy <domain>` (`[CHOSEN, not recovered]`, option id 75, client and dedicated
servers) replaces the domain for all services of a provider that serves the same names:

| Service | Host |
|---|---|
| Availability check, UDP 27900 | `battlefield2.available.<domain>` |
| Master: heartbeats UDP 27900, CD-key validation UDP 29910 | `battlefield2.master.<domain>` |
| Server list, TCP 28910 | `battlefield2.ms14.<domain>` |
| Presence login / search, TCP 29900 / 29901 | `gpcm.<domain>` / `gpsp.<domain>` |
| Game statistics, TCP 29920 | `battlefield2.gamestats.<domain>` |
| NAT negotiation, UDP 27901 | `battlefield2.natneg1.<domain>` to `natneg3` |
| Stats web requests, HTTP | `BF2Web.<domain>/ASP/`, `stage-net.<domain>/bf2/` |

The value must be a domain name; an address or an unusable value ends start-up with a
message. The option is read from the command line only, and a client that restarts itself
to change mods passes it on. At start-up each image prints one line to stderr that names
the host of every service (`online services: domain ...`). Retail's Windows hosts-file check
for `BF2Web.gamespy.com`, which ended the game when the hosts file named that host, is skipped,
so the hosts file can point the names at a replacement service. The implementation is in
`Code/BF2/Game/MatchMaking/OnlineProvider.h`.

Connections to the server list, the presence managers and the stats web service give up after
5 seconds (`[CHOSEN, not recovered]`, see `Code/BF2/External/GameSpy/LOCAL_CHANGES.md`); retail
waited for the operating system's TCP timeout when a name resolved to an address that never
answers.

`bf2_online_provider_check` (macOS, built on request) checks the names for the retail
domain and for another one against the linked SDK, and the connect timeouts against a
loopback listener that never accepts. It replaces the resolver inside the test process and
uses loopback listeners only, so it contacts no other machine.

For the x86 focused suite, use `bf2_run_checks`. The existing batch gates remain
available through `bf2_syntaxcheck` and `bf2_gap_report`. The latter invokes the full
diagnostic build: set `BF2_BUILD_OUT` to a unique directory because that script cleans
its output directory. Do not reuse another contributor's output or logs.

`bf2_runtime_pair` uses the same explicit source list as `Code/Tests/runtime_pair.cmd`
and links without `/FORCE`. When extending this fixture, keep both source lists in sync
so the CMake check used by CI links the same implementations as the batch check.

After configuring with `-B`, build that exact directory as shown above. Build presets
use their preset's default directory (`out/build/<preset>`), not your custom `-B` path.
The authoritative target definitions are in the [root CMake file](../CMakeLists.txt)
and [editor CMake file](../Code/Tools/BF2Editor/CMakeLists.txt).
