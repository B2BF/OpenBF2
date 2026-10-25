# Building the engine and tools

Run commands from the repository root unless a step says otherwise. The checked-in
[presets](../CMakePresets.json) select Ninja and a build configuration; the Windows
presets expect an initialized Visual Studio environment. They do not switch an x64
compiler into x86 mode. In an IDE, select the matching Visual Studio architecture.

## Targets and outputs

GitHub Actions builds the supported Windows and Linux images on pushes and pull requests,
and attaches ZIP packages when a release is published. See [CI and release packages](ci-releases.md)
for the matrix, archive contents, checks and current runtime limitations.

Paths below are relative to the selected CMake build directory.

| Preset | Target | Output | Link contract |
| --- | --- | --- | --- |
| `msvc-x86` | `bf2_recovered` | Static engine archive | Compile check |
| `msvc-x86` | `bf2_w32ded` | `bin/bf2_w32ded.exe` | Diagnostic `/FORCE` |
| `msvc-x64` | `bf2_w64ded` | `bin_w64ded/bf2_w64ded.exe` | Strict |
| `linux-x64` | `bf2` | `bin_linux/bf2` | Strict |
| `msvc-x86` | `bf2_client` | `bin_client/BF2.exe` | Diagnostic `/FORCE` |
| `msvc-x86` | `bf2_editor` | `bin_editor/BF2Editor.exe` | Diagnostic `/FORCE` |

The x86 preset uses `Reconstruction` with `BUILD_TESTING=ON`. Windows x64 uses `Release`;
Linux uses `RelWithDebInfo`; both default to `BUILD_TESTING=OFF`. The client and editor
are explicit targets, available only on Windows x86. The optional `bf2_aidll` target
builds `bin/AIDLL_w32ded.dll`.

A strict link resolves the image's symbols. Runtime behavior still needs validation.
A `/FORCE` link can produce an executable with unresolved calls, so treat it as a
diagnostic artifact and report the exact paths a run exercised.

## Windows prerequisites

- MSVC C++ tools and Windows SDK, with x86 and x64 components as needed.
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
not validate retail x86 offsets.

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

## Client and plugins

Use the x86 configuration and Python runtime above:

```powershell
cmake --build out/build/local-x86 --target bf2_client
cmake --build out/build/local-x86 --target bf2_client_linkcheck
```

`bf2_client` rebuilds and stages `RendDX9.dll`, `SwiffPlayer.dll`, `BF2Audio.dll`, and
`TextureAtlasBuilder.dll` beside `BF2.exe`. `bf2_client_linkcheck` is the strict companion link;
unresolved symbols make it fail and identify remaining work. The main client target's
`/FORCE` output does not imply that the strict companion passes.

The client plugin steps currently share `Code/Build/obj_*_native` and
`Code/Build/bin_*_native` across CMake trees. **Serialize client builds.** The staging
paths are fixed, so redirecting a plugin's `BIN` environment variable during a CMake
client build would make it stage from the wrong location.

Standalone plugin scripts build only for the rebuilt x86 client. No argument and the
optional `native` argument select the same build; other arguments are rejected.
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
```

Default DLL directories remain `Code/Build/bin_renddx9_native`,
`bin_swiffplayer_native`, `bin_bf2audio_native`, and `bin_textureatlasbuilder_native`.
Object directories use `obj_` instead of `bin_`. Existing compatibility outputs are
not selected or removed by these scripts. Standalone builds support these isolation overrides:

| Script | Object directory | DLL directory |
| --- | --- | --- |
| `renddx9.cmd` | `BF2_RENDDX9_OUT` | `BF2_RENDDX9_BIN` |
| `swiffplayer.cmd` | `BF2_SWIFFPLAYER_OUT` | `BF2_SWIFFPLAYER_BIN` |
| `bf2audio.cmd` | `BF2_BF2AUDIO_OUT` | `BF2_BF2AUDIO_BIN` |
| `textureatlasbuilder.cmd` | `BF2_TEXTUREATLAS_OUT` | `BF2_TEXTUREATLAS_BIN` |

TextureAtlasBuilder uses its own static CRT and legacy float rounding to preserve `.tai`
output. It does not transfer ownership of CRT allocations across the DLL boundary;
callback string references are borrowed. The rebuilt plugins
do not depend on `Memory.dll`, `msvcr71.dll`, or `msvcp71.dll`.
The renderer and atlas scripts can use the April 2005 DirectX SDK (D3DX version 25)
through `DXSDK_DIR`; they also have a no-SDK import-library fallback. The runtime still
needs the matching `d3dx9_25.dll`. Client runtime dependencies also include the game's
Bink and Novint components, TextureAtlasBuilder, and an OpenAL provider for audio.
The audio build uses vendored Ogg/Vorbis by default; setting `BF2AUDIO_NO_VORBIS` omits it.

The CMake client target stages all four rebuilt DLLs above. It also stages Python
and `NovintHFX.dll` when present at configuration time. Keep runtime testing in a separate
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

## Editor

The editor uses the x86 client engine and its own application entry point. Dear ImGui
and ImGuizmo are vendored. Build its engine-free checks before the application:

```powershell
cmake --build out/build/local-x86 --target bf2_editor_run_checks
cmake --build out/build/local-x86 --target bf2_editor
cmake --build out/build/local-x86 --target bf2_editor_linkcheck
```

`bf2_editor_run_checks` exercises framework code without loading the engine or game
content. `bf2_editor_linkcheck` reports unresolved image symbols; it may fail while the
diagnostic `bf2_editor` target still emits an executable.

Each editor build normally rebuilds its native renderer and TextureAtlasBuilder under
`<build>/editor_renddx9/` and stages them into `<build>/bin_editor/`. Python and the
available Novint companion are also staged. These renderer outputs are separate from
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
