# macOS port

The macOS port is an intentional platform change requested by the project owner. No
linked issue exists; the owner's request stands in for it. Every macOS-specific build
decision and source arm is `[CHOSEN, not recovered]`: there was no retail macOS image,
and nothing here claims recovered behaviour. Shipped PE/ELF behaviour stays the
reference for everything that is not a platform seam.

The work is planned in three milestones:

1. The engine builds and runs headless on macOS (the dedicated server). This document
   records its result.
2. The client platform layer: window, input and audio.
3. A native Metal renderer behind the RendDX9 interface.

Build instructions are in [building.md](building.md#macos).

This page is the port's running log. It names the slice reports that sat beside it during
development (for example "the G3 package" or "the S2.6 results"); those working notes are
no longer in the repository, and the names remain only as labels for the work.

## Port rules

These rules apply to every native (non-MSVC-x86) arm; source comments cite them.

- **Memory-safety exception.** Shipped defects normally stay, but an out-of-bounds access, an
  uninitialised read or a null dereference that retail survives only because of the Windows
  heap or D3D layout is not reproduced: the native arm keeps the `[CHOSEN, not recovered]`
  omission, and its comment cites the PE address and describes the defect. The native backends
  fault on such defects (restoring `MemeTexture::applyCanvas`'s index -1 write crashed every
  macOS client on the menu map).
- **x87 precision arms.** `bf2x87::wide` stops on every MSVC build (binary64 `long double`).
  Guard an x87-precision arm with `#if defined(__APPLE__)` or `#if !defined(_MSC_VER)`, never as
  the `#else` of an `_M_IX86` test, which MSVC x64 would take. `python3 tools/tracks/x87_reach.py
  msvc-x64` must show no use of `wide`.
- **Inline assembly.** See [Inline assembly (S2.1)](#inline-assembly-s21).
- **Raw vtable slots.** A cross-image call goes through a typed native interface or a named,
  audited adapter; see [c-raw-slots](macos-c-raw-slots.md) and the gate
  `tools/macos/raw_slot_scan.py`.

## PR #55 integration

The owner-requested PR #55 integration replaces S2.7's
temporary native-width vc71 containers with the PR's native STL throughout the
production sources. Its `_M_IX86` retail checks replace competing S2.2 Apple
object-size branches. The historical S2.2/S2.7 measurements below describe their
original stages, not the current container implementation.

On macOS x86_64, `bf2`, `bf2_aidll` and the renderer-excluded `bf2_client` now
compile and link strictly. Of 121 renderer TUs, 101 still fail (100 on missing
Windows/DirectX headers and one on MSVC formatting names). The static-plugin
prelink and private-symbol lists remain necessary. The integration report records
the before/after measurements, conflict resolutions and sandbox restriction that
prevents recording the Git merge parent here. No new startup or gameplay reach
has been tested.

S2.10 rules 5, 6 and 8 (POSIX publication, UTF-16 file boundaries and packed data)
were documented in the S2.10 threads notes, including the
complete volatile/Interlocked inventory, Intel checks and owner-run M2 TSan bundle.

## G2 graphics subset

The owner-requested G2 package compiles all 122
renderer TUs (121 existing plus the native backend boundary) against D3D9/D3DX
subset declarations. `BF2_MACOS_LINK_RENDDX9=ON` now links the renderer into
`bf2_client` strictly, with no unresolved or duplicate symbols. All 32 graphics
free functions diagnose and abort, starting with `Direct3DCreate9`; the client
was not run. Windows forwarding, Metal rendering and native display-mode policy
remain open. The package report records selection, API counts, static-image
sharing, validation and the coordinator startup command. The older missing-header
counts below describe their earlier snapshots.

## G3 Metal device

The owner-requested G3 package replaces the
`Direct3DCreate9` frontier with a native default-device adapter and implements
the default Metal surfaces/swap chain, viewport, clear/present and resize/reset.
It adds a GPU harness under `Code/Tests/macos_metal_device`. Format/caps/mode
queries are explicit native policy; all state/shader/draw and D3DX operations
still stop at named frontiers. G3 uses main-screen mode metadata without mode
switching and supplies the initial D24S8 candidate only after a capability query.
The expected next startup stop is `IDirect3DDevice9::SetRenderState` while the
StateManager reissues defaults, before `initRenderer` returns and before engine
input initialization. The package report distinguishes compilation/link checks
from coordinator-only GPU/client validation and records the exact commands.

The owner-requested G3b package extends that backend
with D3D state/default caches and Metal mappings, vertex declarations, textures
and buffers, locks/uploads/readback/copies, non-default targets and EVENT/empty
OCCLUSION queries. Unsupported state values are retained and refused at draw time.
Draws, shader creation and all D3DX/effect entries remain frontiers. Renderer-ON,
default client and dedicated strict links plus CPU checks pass; GPU harnesses
compile for x86_64 and arm64 but await coordinator runs. Source inspection suggests
D3DXCreateEffect for PortedFontShader is the next startup stop when its content is
mounted; this has not been established by a client run. The G3 SetRenderState stop
above describes the earlier device-only milestone.

## G3c D3DX helpers

The owner-requested G3c D3DX helpers implement the
non-effect inventory over G3b resources: surface conversion, DDS/TGA/BMP/PNG loading,
mip/fill/render-to-surface operations, math/half floats, native meshes and buffers.
The startup AA probe assembler is implemented narrowly; general assembly remains
a named stop. CPU and sanitizer checks pass, and the content scan decodes 10,980
images while explicitly rejecting one non-image. Strict renderer-ON/default client
and dedicated builds pass. GPU tests compile but await coordinator execution.
No client runtime advance is claimed; the predicted next stop is D3DXCreateEffect
for PortedFontShader. Effect code is unchanged. The package report lists remaining
stops, numerical/codec limits and coordinator commands.

## Menu backend (P6)

The owner-requested P6 menu backend package implements
partial/oversized viewport and scissor clears, Present flags/window overrides and
ImageIO JPEG loading through G3c. CPU/reference/sanitizer checks pass. Its GPU
groups are prepared for coordinator execution; see the package report for build
results, API restrictions and remaining frontiers.

The Cocoa layer uses nearest-neighbour magnification for internal-resolution
drawables (`[CHOSEN, not recovered]`) so Retina enlargement preserves bitmap text
edges. The retail font-path comparison and compositor validation are still pending.

The login movie preselects the directory saved in `GlobalSettings.defaultUser`.
The PREFIX selection report records the retail native and
movie evidence, the harness's missing saved-default setting, and a coordinator
capture command for startup and Manage -> Back without profile clicks. The
[profile case follow-up](macos-profile-prefix.md) explains why real installs still lost the
selection (a lowercased `Profiles/default`) and adds the two-start covering check.

The owner-approved offline Connect to IP policy
observes the main menu's explicit, visible `selectedPopup="connecttoip"` request
after ActionScript runs and selects its existing dialog for login status 1.
It replaces the earlier callback wrapper, which did not fire in the owner capture.
This intentional client policy applies on every rebuilt platform; profile status,
movie files, native joining and packet behavior are unchanged. Live dialog/join
validation remains coordinator work.

## Offline shader coverage (S1c)

The owner-requested S1c coverage work keeps all engine
and MSVC paths unchanged. In the selected offline corpus, 219/269 D3DX attempts
compile (including two original controls), all 219 effects parse, and 982/982
embedded shaders translate and compile as Metal libraries on Intel Iris Plus 655.
The remaining 50 compiler refusals are real lower-profile attempts that precede
engine fallback. One inactive Ra template fragment is excluded with its source
contract recorded. Three separate regression effects add eight passing libraries.

Named shader references, fixed array references and SM1 texture instructions now
have local MojoShader patches. Dynamic state expression payloads are retained
and inventoried; executing one still stops at a named evaluator frontier. This
is a shader-library result, not a native ID3DXEffect, pipeline or draw result.

## Milestone 1 status

Measured on macOS 15.7.7 (Darwin 24.6.0), Intel i7-8569U (x86_64), Apple clang 17.0.0
from the Command Line Tools, CMake 4.4, Unix Makefiles.

| Stage | Result |
| --- | --- |
| Compile, `bf2` (x86_64) | Pass. About 6 minutes from clean with `-j4`. |
| Link, `bf2` (x86_64) | Strict `ld64` link, no unresolved symbols. Links `@rpath/libdice_py.dylib`, the SDK's ncurses, libc++ and libSystem. |
| `dice_py` / `dice_py_check_run` | Pass: interpreter start, 64-bit `PyCObject` round trip, 40 builtin modules, zlib, loopback UDP. |
| arm64 cross-compile (`-DCMAKE_OSX_ARCHITECTURES=arm64`) | `bf2`, `libdice_py.dylib` and `dice_py_check` compile and link. Not run: no Apple silicon machine was available. |
| Production startup | Reached through `main -> BF2::init -> BF2Engine::startGame -> GameServer`. |
| Socket listener | Reached: game port bound on `127.0.0.1:16890`, GameSpy LAN query port on UDP 29911. |
| Map load | Reached: `dalian_plant`, `gpm_cq`, 16-player layer. The status monitor shows `Status: [playing]`; a stack sample shows `BF2Engine::mainLoop -> GameServer::update -> simulateFrame -> TriggerManager::handleUpdate`. |
| Query response | A qr2 basic-info query on loopback returns host name, map `Dalian Plant`, mode `gpm_cq`, `gamemode openplaying`, host port 16890. |
| Shutdown and rebind | `quit` typed into the ncurses console exits with status 0 within 0.2 s and frees the port. A second run immediately afterwards bound the same port and reached the same state. |
| Joined player | Not exercised. No client connected. |

### Smoke configuration

- Content: the owner's retail Battlefield 2 1.50 Windows installation inside the Wine
  bundle in `~/Applications`, used read-only. `mods/`, `python/` and
  `pylib-2.3.4.zip` were copied with APFS clones (`cp -Rc`) into
  `out/smoke/root`, which was the working directory. A post-run search found no file
  in the reference installation newer than the run.
- Command line:
  `bf2 +modPath mods/bf2 +config smoke_cfg/ServerSettings.con +mapList smoke_cfg/maplist.con`,
  with `+noStatusMonitor 1` for the unattended run and the status monitor in a
  pseudo-terminal for the `quit` runs.
- Settings: the stock `ServerSettings.con` with `sv.serverPort 16890`, `sv.internet 0`,
  `sv.serverIP` and `sv.interfaceIP 127.0.0.1`, `sv.gameSpyPort 29911`,
  `sv.voipEnabled 0`, `sv.useGlobalRank 0`, `sv.useGlobalUnlocks 0`,
  `sv.autoRecord 0`. Map list: `mapList.append dalian_plant gpm_cq 16`.
- Isolation: every run was wrapped in `sandbox-exec` with a profile that denies all
  outbound traffic except loopback, denies the mDNSResponder socket (no DNS), denies
  writes to the reference installation and denies binding port 16567. The GameSpy
  availability check therefore failed its lookup and the SDK's fail-open path brought
  up qr2 in LAN mode; no external host was contacted.
- Durations: one unattended run of about 3.5 minutes in the main loop, ended with
  `SIGTERM`; two pseudo-terminal runs of 120 seconds each, ended with `quit`.

An earlier attempt passed `+config` and `+mapList` as absolute paths. The server
ignored them, started with the mod's default settings, and bound UDP 16567 for well
under a minute before it was stopped. Nothing else was bound to that port at the time.
The 16567 bind denial in the sandbox profile was added afterwards. Pass both paths
relative to the content root.

Not exercised in milestone 1: client joins, bots (S2.6 now builds a native AI image;
its server smoke remains unvalidated),
map rotation and reload, ranked or internet mode, PunkBuster, VoIP, demo recording,
remote administration, and long soaks.

## Stage 1a: universal build and M2 validation

Stage 1a and S2.0 item 4 follow the owner's [macOS plan](../macOSplan.md).
These are `[CHOSEN, not recovered]` build and validation changes. `macos-x64`
explicitly selects x86_64; `macos-universal` inherits its Unix Makefiles, SDK and
configuration settings and selects `arm64;x86_64`. Python's existing compiler-macro
`va_list` override applies independently to each slice; no engine behavior or
MSVC source arm is changed.

The reproducible M2 bundle is built with:

```bash
cmake --preset macos-universal -B out/build/s1a-arm64
cmake --build out/build/s1a-arm64 --target bf2 dice_py dice_py_check bf2_aidll bf2_aidll_loader_check -j4
bash Code/Tests/macos_stage1a/package.sh out/build/s1a-arm64
```

`out/m2/stage1a/` is ignored by Git and contains the five Mach-O artifacts, the
two Python check modules, `run.sh`, `smoke.tcl`, `offline.sb`, `README.txt` and
artifact provenance/dependencies. Copy the whole folder to the M2 and follow its
README. No game content is distributed. The first argument to `run.sh` is the
read-only Wine content root; the script clones `mods/`, `python/` and
`pylib-2.3.4.zip` with `cp -Rc` under the bundle before running the server.

The smoke uses the milestone 1 sandbox rules, with the protected reference path
supplied as a profile parameter: outbound loopback only, no mDNSResponder socket,
no reference writes and no binding port 16567. It refuses occupied UDP 16890/29911,
uses relative offline configuration paths, and runs the real dedicated executable
in a PTY for 60 seconds before sending `quit`. A premature exit or a 30-second
shutdown timeout fails visibly. The script records machine architecture/model,
exit codes and each log's last 50 lines in `results.txt`; full logs stay beside it.
The Python payload is the same executable/input as `dice_py_check_run`; the AI
loader check does not call `initDll` or prove bot behavior.

The agent sandbox rejects nested `sandbox-exec` with
`sandbox_apply: Operation not permitted`; it also denies `sysctl hw.model`.
There is no unsandboxed server fallback. Native M2 execution remains owner work;
compile/link results alone do not complete stage 1a.

Measured in this worktree (Apple clang 17, macOS 15.7, Intel host):

| Validation | Result |
| --- | --- |
| `macos-x64` configure | Pass; cache architecture is explicitly x86_64. |
| Universal explicit five-target build and default `cmake --build ... -j4` | Pass, exit 0; strict executable/dylib links. No engine source fixes were required. |
| `lipo -archs` on `bf2`, `dice_py_check`, `bf2_aidll_loader_check`, `libdice_py.dylib`, `libaidll.dylib` | All contain x86_64 and arm64. |
| Bundle `arch -x86_64` rehearsal | AI loader exit 0. Python check exit 2 at the loopback bind (`Operation not permitted`), after initialization, pointer round trip, builtin imports and zlib work. |
| Sandbox preflight | Exit 71 (`sandbox_apply: Operation not permitted`); server not launched, no content clone created. |
| Bundle overall exit | 1; no full Python runtime or server-smoke pass is claimed. |
| Native M2 runtime | Pending owner execution of `out/m2/stage1a/README.txt`. |

The universal artifacts depend only on bundled `libdice_py` and macOS system
libraries. Copied-bundle loading was exercised by the checks; `otool` also checked
the arm64 dependencies. The local rehearsal is retained as `results-x86_64.txt`
alongside `results.txt` in the bundle. No production server path ran in stage 1a.

### arm64 measurement

The arm64-only measurement uses the actual client manifest, with the renderer
disabled, `DEDICATED=0`, force-included build/platform headers, C++14, libc++ and
Apple clang 17:

```bash
cmake --preset macos-x64 -B out/build/s1a-arm64-client -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build out/build/s1a-arm64-client --target bf2_client -j4 -- -k
```

Detailed results and remaining work are recorded in
the stage 1a report. The keep-going build exits 2 before the
client link. Of 1,016 client/plugin/vendor commands (Python and the disabled
renderer excluded), 984 produced objects; three host TUs blocked by prerequisites
pass syntax checking. The remaining 29 TUs have 61 errors. A second pass over all
32 missing objects used their actual compile commands with `-fsyntax-only
-ferror-limit=0`, once per architecture, without suppressing assertions.

**No arm64/x86_64 size difference was found among the evaluated assertions.** All
180 S2.2 native assertion sites retain their existing values. The three previously
deferred assertions fail identically on both architectures:

| Check | Existing expected size | x86_64 actual | arm64 actual |
| --- | ---: | ---: | ---: |
| `SwiffPlayer.cpp:86`, `SwiffPlayer` | 1120 | 1176 | 1176 |
| `SwiffPlayer.cpp:528`, `LocalProfileRecord` | 176 | 200 | 200 |
| `BF2Render.cpp:108`, `RendererDisplayMode` | 40 | 72 | 72 |

| Failure class | arm64 TUs | arm64 errors/sites | x86_64 errors in the comparison |
| --- | ---: | ---: | ---: |
| MS-style inline assembly (arm64-only) | 27 | 58 | 0 |
| Deferred size assertions (shared) | 2 | 3 | 3 |
| Other arm64-only classes | 0 | 0 | 0 |

Assembly failures comprise BFMeme (8 TUs / 12 blocks), Menu (18 / 42) and Math
(1 / 4). No fatal include hides additional errors in this pass. This measures the
current client target, including SwiffPlayer and BF2Audio, without the stage 3
renderer trees; it does not validate assembly that Clang accepts on x86_64.
Local diagnostics, the measurement driver and the JSON inventory are under
`out/build/s1a-arm64-logs/` (`build-client-arm64.log`, `triage.py`, `triage.json`,
`triage-arm64/`, `triage-x86_64/`). Inline-assembly conversion belongs to
S2.1. Precision, float-to-integer semantics, atomics, on-disk `wchar_t` and native
M2 ThreadSanitizer validation remain S2.10; a successful compile cannot check them.

## What changed and why

| Area | Change | Reason |
| --- | --- | --- |
| `CMakeLists.txt` | `BF2_BUILD_POSIX` family with `BF2_BUILD_LINUX` and `BF2_BUILD_MACOS` members. | Linux blocks that are really POSIX now serve both ports; only Linux-specific choices stay under `BF2_BUILD_LINUX`. MSVC and Linux settings are unchanged. |
| `CMakeLists.txt`, `Code/AidllAbi/`, `AI/AIDLL/AI/AIBot/BotMain.h` (S2.6) | Native POSIX `bf2_aidll`, private symbols, explicit `fullInvert` stop and fixed-underlying-type enum definition. | No AI frontier bypass. |
| `CMakeLists.txt` | macOS: `_MACOSX` for the GameSpy SDK, `BF2_PLATFORM_POSIX` for engine TUs, SDK ncurses (no wide variant), `@loader_path` rpath, `-Wl,-map`, output in `bin_macos/`. | Darwin's compilers do not define `__unix__`, which `UnixFileSystem`, `UnixFileStream` and the POSIX status monitor tested. The SDK has no `libncursesw`. `$ORIGIN` and `-Map=` are ELF spellings. |
| `CMakePresets.json` | `macos-x64` configure and build presets. | Unix Makefiles (no Ninja), `RelWithDebInfo`, `CMAKE_OSX_SYSROOT=macosx`. CMake 4 no longer sets a sysroot by default, and without one Apple clang searches `/usr/local/include` (Homebrew) first. |
| `External/python-embedded/linux/CMakeLists.txt` | Accepts Darwin and selects `../macos/pyconfig.h`; `-U__APPLE__` for `_localemodule.c`; no `--no-undefined` on macOS. | See [macos/README.md](../Code/BF2/External/python-embedded/macos/README.md). |
| `External/python-embedded/macos/pyconfig.h` | Configure output for Darwin x86_64, with `HAVE_BROKEN_POSIX_SEMAPHORES` and an arm64 `VA_LIST_IS_ARRAY` override. | Darwin's `sem_init` fails with `ENOSYS`; arm64 `va_list` is a pointer. |
| `External/GameSpy/common/gsPlatformSocket.h` | `_MACOSX` socket-call macros apply to C only. | In C++ the function-like `connect` macro rewrote `StatsManager::connect`. Recorded in `External/GameSpy/LOCAL_CHANGES.md`. |
| `Core/PluginDll.cpp`, `Game/Main/OpenBF2Console.cpp` | `lib<name>.dylib` on macOS. | Shared-library suffix. |
| `IO/File/FileManager.h` | libc++ container-width assertions. | The existing native branch asserts libstdc++ widths (`std::string` is 24 bytes in libc++). |
| `Core/System.cpp`, `Animation/BoneAnimation/RagDollTemplate.cpp` | `cntvct_el0` read on `__aarch64__`. | The POSIX timestamp helper used x86 `rdtsc` inline assembly. |
| `Core/ClassFactoryInstantiations.cpp` | Removed a duplicate explicit instantiation. | Clang rejects it; GCC accepted it under `-fpermissive`. Toolchain-neutral. |
| `IO/Network/.../UDPSocket.cpp`, `ProtocolLayer/NetServer.cpp` | `htons`/`htonl` called unqualified. | They are macros on Darwin, so `::htons` does not parse. Toolchain-neutral. |
| `Sound/ObjectSoundNode.cpp` | `static const Mat4 s_zero{}`. | Clang rejects default-initialising a const object without a user-provided constructor. Static storage was zero either way. |
| `CMakeLists.txt` (S2.0) | macOS `bf2_client`, `bf2_renddx9_static`, `bf2_swiffplayer_static`, `bf2_bf2audio_static`, `bf2_plugin_kit_static`, `bf2_vorbis_static`, `bf2_voipserver_client_objects`; POSIX defines on `bf2_client_objects`. | The client target; plugins linked statically because DLL swapping is retired (issue #7). |
| `Code/StaticPlugins/`, `Core/PluginDll.cpp/.h` (S2.0) | Static plugin table consulted by the POSIX loader wrappers; wrappers declared in `PluginDll.h`. | The engine's `LoadLibraryBf`/`GetProcAddress` call sites stay unchanged. |
| `Core/ModuleRegistry.cpp` (S2.0) | Non-Win32 clients load BF2Audio through the POSIX wrappers. | The client arm included `<windows.h>` for `LoadLibraryA`. |
| `RendDX9Abi/PluginAbi.h` (S2.0) | The `/MD` gate accepts `BF2_STATIC_PLUGINS`. | Statically linked plugins share the host CRT by construction. |
| `Game/Main/BF2.h`, `Menu/Bf2HudBuilder.cpp`, `External/base/tu_types.h`, `tu_math.h` (S2.0) | Console-ring declarations for every client; `!= nullptr`; x86-64/AArch64 as little-endian; OS X float macros off. | Toolchain-neutral compile fixes; see the triage. |
| `CMakeLists.txt`, `Core/ModuleRegistry.cpp` (S2.7) | `OBJCXX` on Darwin; `CocoaHost.mm`/`MacGamepad.mm` (ARC) and six frameworks in `bf2_client`; DirectInput TUs out of the macOS client manifest; `MacInputDeviceManager` under the DX8 CID on `__APPLE__`. | Wires package S2.4 into the client target. |
| `RendDX9Abi/Vc71Layout.h`, `Vc71Vector.h`, `Vc71String.*`, `Vc71Map.h` (S2.7) | `BF2_VC71_RETAIL_LAYOUT`; x86 size checks on 32-bit builds only; native-width `set_key` and map value offset; `[FRONTIER]` stops in the raw x86 node-size helpers. | [vc71 on native ports](#vc71-on-native-ports). MSVC x86 unchanged. |
| `RendDX9Abi/DisplayModeAbi.h` (S2.7) | MSVC x86 carrier checks skipped under `BF2_STATIC_PLUGINS`. | One image, one STL. |
| `CMakeLists.txt`, `Code/StaticPlugins/*-private-symbols.txt` (S2.7) | `ld -r` prelink of SwiffPlayer, BF2Audio (and RendDX9 when linked) with image-private globals. | [Plugin globals](#plugin-globals). |
| `External/base/utility.h`, `container.h`, `gameswf/gameswf_{types,text,impl}.cpp` (S2.7) | `fmax`/`fmin`/`log2(float)` in namespace `tu_util`, called qualified. | libc++ declares the C library overloads globally; gameswf keeps its semantics. Toolchain-neutral. |

No Linux-only API (`epoll`, `prctl`, `memalign`, `malloc.h`, `clock_gettime` clocks,
`/proc`) is used by the engine outside `External/`; the POSIX arms already used
`select`, `pthread`, `gettimeofday`, `getrusage`, `dlopen` and `realpath`, all
available on Darwin. The GameSpy SDK's own `_MACOSX` branch is used as shipped.

### Remaining milestone 1 items

- **Linux and MSVC were not rebuilt.** This machine has neither toolchain. The CMake
  changes keep their branches' settings, and the shared source edits are
  toolchain-neutral, but run the `linux-x64` build and the MSVC gates before merging.
- **`bf2_os` reports `linux-64`.** `BuildNrUtil.cpp:getOS` returns the retail Linux
  value for every non-Windows build, and the query response advertises it. Whether a
  macOS server should advertise a different value is an owner decision; changing it
  may affect server-browser filters.
- **No signal handling.** The server has no `SIGINT`/`SIGTERM` handler on either POSIX
  port, so an unattended server can only be stopped without the shutdown path.
  Console input exists only through the ncurses status monitor.
- **Absolute `+config` paths are ignored.** Observed on macOS, as described above;
  check whether Linux behaves the same and whether that matches the retail ELF.
- **Clang diagnostics to audit.** Clang reports `if (this)` checks
  (`-Wundefined-bool-conversion`, retained retail guards), deletes through incomplete
  types (`BF2Engine.cpp` for `PerformanceLogging`, `BF2Window`, `BF2Render`;
  `BFGraphCreator.cpp`), and deletes through `void*`. `-fno-delete-null-pointer-checks`
  is passed; a small test showed Clang keeping a `this` null check under `-O2`, but the
  guarantee is weaker than GCC's. The incomplete-type deletes skip destructors on every
  toolchain; they are recorded here, not changed.
- **arm64 is compile- and link-tested only.** Run `dice_py_check_run` and the smoke on
  Apple silicon before claiming it.

## What milestones 2 and 3 need from milestone 1

- The `BF2_BUILD_MACOS` switch, the preset and the libc++/AppleClang fixes apply
  unchanged to a client image. Package S2.0 added the macOS `bf2_client` target and the
  static plugin libraries; see [Client triage](#client-triage-package-s20).
- The macOS `dice_py` and `@loader_path` layout carry over; the client uses the same
  interpreter.
- The plugins are static libraries in the macOS client. `PluginDll.cpp`'s POSIX wrappers
  answer them from `Code/StaticPlugins` first and fall back to `lib<name>.dylib`.
- The smoke harness (`sandbox-exec` profile, pseudo-terminal `quit`, cloned content
  root) is reusable for a local client-to-server loopback test.

## Frontier for milestone 2: the client platform layer

The window host and input layer (package S2.4) are described in
[macos-s2-4.md](macos-s2-4.md): the Cocoa host, the macOS input-device manager, the
`DIK_` table, the engine wiring and what the client target still has to connect.

The client image is built today only by the MSVC x86 configuration, with `/FORCE`, and
its plugins are separate 32-bit Windows DLLs. These are the platform dependencies a
macOS client must replace. Sizes are counted in this tree; estimates are rough
engineer-weeks for a first working version and assume the 64-bit blockers below are
handled separately.

| Item | Where | Size | Effort |
| --- | --- | --- | --- |
| Entry point and window: `WinMain`, `BF2Window`, message pump, desktop-mode save/restore | `Game/Main/BF2.cpp`, `BF2Window.cpp/.h`, Win32 calls in `BF2Engine.cpp`, `MainMenuLogic.cpp` | About 12 files reference `HWND`/window APIs | 1-2 weeks: a Cocoa `NSApplication`/`NSWindow` host with a `CAMetalLayer`, plus focus and fullscreen handling |
| Keyboard and mouse: DirectInput 8 device managers and `DIK_` scan codes | `IO/Input/DX8InputDeviceManager`, `KeyboardDevice`, `MouseDevice`, `GameControllerDevice`, `FalconDevice` (about 4,700 lines in `IO/Input`) | 8 files use DirectInput types | 1-2 weeks: map `NSEvent`/raw mouse deltas (or `GCKeyboard`/`GCMouse`) onto the existing `DIK_` codes so control maps and `.con` bindings stay valid. `NullInputDeviceManager` already exists for headless use. |
| Joysticks and the Novint Falcon | `GameControllerDevice`, `FalconDevice`, `External/NovintHFX` | Small | Days for `GCController`; the Novint SDK is Windows-only and should stay disabled |
| Audio: BF2Audio plugin | `Sound/SoundEngine` (OpenAL provider, about 11,400 lines in `Sound/`), vendored Ogg/Vorbis | One DirectSound file (`Sound/SoundEngine/Win32System.cpp`) | About 1 week: the engine already speaks OpenAL. macOS ships `OpenAL.framework` (deprecated but present); OpenAL Soft is the long-term option and would need installing or vendoring. |
| Movie and UI player: SwiffPlayer | `Menu/Swiff*`, `Geom/SwiffRender.cpp`, `External/gameswf`, `External/base`, `SwiffPlayerAbi` | 41 files reference the Swiff interfaces | Platform-light once the renderer exists; it draws through `ISwiffRender`. The plugin build and its `vc71::` host containers need porting (see the 64-bit list). |
| Bink video | `GeomMisc/MenuRend/Video.cpp`, `ModuleGeomMisc.cpp` | Proprietary `binkw32.dll` | macOS: vendored FFmpeg Bink decoders behind the same API (`Code/BF2/External/ffmpeg-bink/README.md`). |
| Clipboard, cursor, gamma ramp, display-mode enumeration | `RendDX9/VideoOptions.cpp`, `RendDX9Init.cpp`, `Game/Main` | Small | Days, as part of the window host |
| Voice (BF2VoipClient) | `VoipServerAbi`, client voice code | Windows audio capture | Defer; the sound-seam and `voipStopTest` exceptions already let the client continue without it |

## Client triage (package S2.0)

Measured on 2026-10-22 on the milestone 1 machine (Apple clang 17, CMake 4.4, Unix
Makefiles, x86_64, `RelWithDebInfo`), branch `mac/s2-0-client`. Every number below is a
compile result of this tree, not an estimate, unless it says otherwise.

### Target status

| Item | State |
| --- | --- |
| Configure | `cmake --preset macos-x64 -B out/build/<dir>` defines the macOS client targets. MSVC and Linux targets are unchanged. |
| `bf2_client` (macOS) | `bin_macos/bf2_client` beside `bf2` and `libdice_py.dylib`, `@loader_path` rpath, strict `ld64` link. `EXCLUDE_FROM_ALL`. S2.11 now builds and links the normal x86_64 target with all assertions enabled and zero unresolved link symbols; startup remains unvalidated because the execution sandbox refused `sandbox-exec`. See [S2.11](#client-startup-s211). Historical triage below: 284 of 1,178 TUs failed at S2.0, 228 of 1,181 after S2.7, and 213 of 1,185 after S2.5/S2.8. S2.9's successful link used measurement objects. |
| Client objects | `bf2_client_objects`, DEDICATED=0, now with `BF2_PLATFORM_POSIX`, `_MACOSX` and `BF2_STATIC_PLUGINS=1` on macOS. 886 TUs, 763 compile. |
| Plugin images | Static libraries: `bf2_renddx9_static` (120 TUs), `bf2_swiffplayer_static` (40), `bf2_bf2audio_static` (16), plus `bf2_plugin_kit_static` (the RendDX9Abi heap/vc71-string/frontier kit, 4) and `bf2_vorbis_static` (Ogg/Vorbis, 23 C). Manifests copied from `Code/Build/{renddx9,swiffplayer,bf2audio}.cmd`; keep them in step. |
| Plugin loading | `Code/StaticPlugins/StaticPlugins.cpp` holds a name table (`RendDX9`, `SwiffPlayer`, `BF2Audio`). The POSIX `LoadLibrary`/`GetProcAddress`/`FreeLibrary` wrappers in `Core/PluginDll.cpp` consult it before `dlopen`, so `LoadLibraryBf("SwiffPlayer.dll")` and `BfSoundModule::init`'s BF2Audio lookup work unchanged. The per-image `initDll`/`deinitDll` pairs are renamed by compile definition (`bf2_renddx9_initDll`, `bf2_swiffplayer_initDll`); BF2Audio's `getSoundEngine`/`shutdownSoundEngine` keep their names. |
| Renderer | Not linked by default (`BF2_MACOS_LINK_RENDDX9=OFF`): the image is Direct3D 9 throughout. Without its row, `LoadLibraryBf("RendDX9.dll")` returns null and `BF2Render::init` takes its shipped failure return. That is the honest null renderer until stage 3; no fake renderer is registered. |
| Input | Since S2.7's wiring: `MacInputDeviceManager` (package S2.4) is registered under the DX8 CID and name in `InputDX8Module::getClassList` on macOS; the five DirectInput TUs (`DX8InputDeviceManager`, `KeyboardDevice`, `MouseDevice`, `GameControllerDevice`, `FalconDevice`) are removed from the macOS client manifest; `CocoaHost.mm` and `MacGamepad.mm` build as ARC Objective-C++ in `bf2_client` (`OBJCXX` enabled on Darwin) and the target links AppKit, QuartzCore, Metal, GameController, Carbon and IOKit. All S2.4 TUs and `ModuleRegistry.cpp` compile in the `macos-x64` client build. |
| TextureAtlasBuilder | Not built. Its only caller is the renderer. |
| Server regression check | After these changes `bf2` (macOS server) rebuilt from clean in the same tree and linked strictly. Not run. Linux and MSVC were not built (no toolchain on this machine); every shared-source change is behind `!defined(_WIN32)`, `BF2_STATIC_PLUGINS` or `BF2_BUILD_MACOS`, or is toolchain-neutral (`BF2.h`, `Bf2HudBuilder.cpp`). |

Build and reproduce:

```bash
cmake --preset macos-x64 -B out/build/s20-x64
cmake --build out/build/s20-x64 --target bf2_client -- -k -j6   # expected to fail; -k compiles the rest
```

The triage itself used a per-TU driver over `compile_commands.json` with
`-ferror-limit=0` (`out/build/s20-x64/triage/`, not checked in), so every TU has its
own complete log.

### How to read the counts

- **First blocker** is the class of a TU's first error. A missing header (`windows.h`,
  `d3d9.h`) is fatal in Clang and hides everything behind it; failed
  `static_assert`s are not fatal.
- **Behind the assertions**: every failing TU was compiled again with `static_assert`
  neutralised (a measurement-only `-include`, never part of a build). 131 of the 284
  then compile; the other 153 have a non-assertion blocker. This is the work left after
  S2.2 and the vc71 assertions.
- **Inline assembly does not fail on x86_64.** Clang accepts MSVC `__asm` blocks for
  x86_64 and assembles them with their 32-bit operands (`mov eax, ptr` truncates a
  64-bit pointer). Those TUs compile and would misbehave at run time. An arm64
  `-fsyntax-only` pass ("unsupported architecture 'arm64' for MS-style inline
  assembly") measures them instead.

### Counts per class

| Class | Owner | TUs, first blocker | TUs with any error | Errors | Distinct sites |
| --- | --- | ---: | ---: | ---: | ---: |
| Inline asm | S2.1 | 0 | 0 on x86_64; **25 TUs / 53 blocks** on arm64 | — | 53 |
| vc71 containers (layout assertions) | S2.7 | 121 | 122 | 318 | 8 in 3 files |
| Size assertions | S2.2 | 106 | 135 | 383 | 218 in 125 files |
| Calling conventions | S2.3 | 0 | 0 | 0 | 0 |
| Win32/DirectX API | S2.4, S2.5, stage 3 | 26 | 125 | 129 | 29 in 27 files |
| D3DX | stage 3 | 0 | 0 (hidden behind `windows.h`) | 0 | — |
| Bink/Novint | S2.8 | 2 | 4 | 4 | 1 (`NovintHFX.h`) |
| Other | see below | 29 | 31 | 154 | 2 files |
| **Total failing** | | **284** of 1,178 | | | |

The full run predates the `base/tu_math.h` fix: 62 of the 154 "other" errors were
`SwiffPlayer.cpp`'s `renddx9abi::logf` renamed by that header's macros, and a re-check
of that TU after the fix leaves its assertions, the `utility.h` conflict and the MSVC CRT
calls. With the assertions neutralised, the 153 TUs that still fail are: Win32/DirectX 119
(97 of them renderer), other 30 (SwiffPlayer), Bink/Novint 4.

### Counts per directory

"Still fail" is the count after neutralising `static_assert`.

| Group | TUs | Fail | vc71 | Size | Win32/DX | Bink/Novint | Other | Still fail |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| client `Core/` | 27 | 1 | 0 | 0 | 1 | 0 | 0 | 1 |
| client `External/BFMeme/` | 58 | 57 | 0 | 57 | 0 | 0 | 0 | 0 |
| client `Game/` | 375 | 8 | 0 | 1 | 6 | 1 | 0 | 7 |
| client `IO/` | 52 | 5 | 0 | 0 | 4 | 1 | 0 | 5 |
| client `Menu/` | 95 | 52 | 0 | 47 | 5 | 0 | 0 | 10 |
| `bf2_bf2audio_static` | 16 | 4 | 3 | 0 | 1 | 0 | 0 | 1 |
| `bf2_plugin_kit_static` | 4 | 4 | 3 | 0 | 1 | 0 | 0 | 1 |
| `bf2_renddx9_static` | 120 | 119 | 111 | 1 | 7 | 0 | 0 | 97 |
| `bf2_swiffplayer_static` | 40 | 34 | 4 | 0 | 1 | 0 | 29 | 31 |
| everything else | 391 | 0 | | | | | | 0 |
| **total** | 1178 | 284 | 121 | 106 | 26 | 2 | 29 | 153 |

Compiling cleanly: client `AI/`, `Animation/`, `Geom/`, `GeomMisc/`, `Math/`, `Physics/`, `RendDX9/`, `RendMisc/`, `Scene/`, `Sound/` (the renderer and audio TUs in these client directories are empty outside their own image), the GameSpy, zlib and TinyXML objects, `bf2_vorbis_static`, the client voice-server objects and `StaticPlugins.cpp`.

### Inline assembly (S2.1)

53 unguarded MSVC `__asm` blocks in 25 TUs fail on arm64 (line numbers are the block
starts). All compile on x86_64 with 32-bit operands, which is a run-time hazard, not a
pass.

- `External/BFMeme/` (12 blocks, 8 TUs): `AlphaFadeEffect.cpp:34`, `CullNode.cpp:43,61`,
  `Data.cpp:52,81`, `MovePointer.cpp:18,29`, `Quad.cpp:78`, `SetVariableSineAction.cpp:21`,
  `SetVariableSoftAction.cpp:19`, `TransformNode.cpp:41,66`.
- `Math/Mat4Inverse.cpp:13,23,34,62` (client-only TU).
- `Menu/` (35 blocks, 15 TUs): `BF2HudManager.cpp:73,97`,
  `BF2HudManagerAbilities.cpp:38,60,77`, `BF2HudManagerChatInput.cpp:26`,
  `BF2HudManagerInput.cpp:16,30,43,60`, `BF2HudManagerPointer.cpp:16,27`,
  `BF2HudManagerProjection.cpp:17,48,67`, `BF2HudManagerSpottedTarget.cpp:63,80,85,101,131`,
  `Bf2CullNode.cpp:48,65`, `Bf2TransformNode.cpp:20,34`, `BfListBoxData.cpp:24,687`,
  `BfNewListBoxNode.cpp:27`, `BfTransformNode.cpp:38,63,77`, `HudCommander.cpp:76`,
  `HudConsoleBindings.cpp:1403`, `HudQuestionManager.cpp:48,56,58`.
- Renderer: `RendMisc/RenderView.cpp:262,354`.

Hidden behind a fatal include, so not yet measured: `Menu/BF2HudManagerUpdate.cpp`
(4 `__asm` mentions), `Menu/HudAmmo.cpp` (2), `Menu/HudVehicleInformation.cpp` (1), and in
the renderer `RendDX9/BF2Render.cpp` (2), `Geom/Overgrowth/OverGrowthCells.cpp` (1),
`Geom/ParticleSystem/ParticleSystemManager.cpp` (2) with its four `Particle*.inl`
includes. `Math/Frustum.cpp`, `Menu/BF2HudManagerHudEvents.cpp` and `External/base/utility.*`
mention `__asm` but are already guarded. The three AIDLL files (`AI/AIDLL/AI/AIBot/Squad*`)
are not part of the client; they matter to S2.6.

S2.1 gives 69 preserved source blocks native alternatives, and a full arm64 client/plugin
syntax scan reports zero assembly errors. The rule behind every
`Preserve the S2.1 POSIX numeric arm` comment:

- Every MSVC `__asm` block stays byte-for-byte unchanged and is compiled only under
  `defined(_MSC_VER) && defined(_M_IX86)`; the C++ alternative is selected otherwise. The helper
  header is not included by MSVC x86.
- `Code/BF2/Math/PortableX87.h` holds the native expressions. They preserve the named
  float/double stores, wider intermediates between stores, operation order, signed integer loads
  and explicit truncation. Conversions check the exclusive power-of-two bound before any C++
  cast and return the x87 integer-indefinite value (raising `FE_INVALID`) for NaN, infinity and
  overflow; FRNDINT uses the active rounding direction.
- Comparisons keep the source's `TEST AH` masks (ordered `<`/`<=`/`>` versus "`>=` or
  unordered"); FCOM/FTST paths raise invalid for quiet NaNs, the RenderView FUCOM zero test stays
  quiet.
- Darwin x86_64 long-double sine/cosine were checked against host x87 instructions; out-of-range
  FSIN/FCOS operands are returned unchanged, as on x87.
 Other compile blockers still prevent a complete client build.
S2.10 numeric compatibility consolidates conversions and
comparisons in `Platform/X87Compat.h`, supplies software extended arithmetic on
Apple silicon, and audits the 41 conversion files and nine long-double files.
Intel hardware/software differential checks pass; the standalone arm64 test
bundle is in `out/m2/numerics/`, with its M2 run still pending.

### vc71 containers (S2.7)

Resolved on `mac/s2-7-vc71`; see [vc71 on native ports](#vc71-on-native-ports) and
[Re-measurement after S2.7](#re-measurement-after-s27). The S2.0 measurement follows.

Measured only in the plugin images; the client objects have none. Eight layout
assertions in three files, reached through `RendDX9Abi/PluginAbi.h`:

| Site | Check | TUs |
| --- | --- | ---: |
| `RendDX9Abi/Vc71Vector.h:31` | `sizeof(vc71::vector_layout) == 16` | 121 (every plugin TU including `PluginAbi.h`) |
| `RendDX9Abi/Vc71Map.h:30, 186, 202, 471, 613` | map/set node and header sizes (48, 12, 20, 12, 40) | 39 |
| `RendDX9Abi/Vc71String.cpp:14, 168` | `sizeof(vc71::string/wstring) == 28` | 1 |

Affected: `bf2_renddx9_static` 112 TUs, `bf2_swiffplayer_static` 4
(`SwiffPlayer.cpp`, `SwiffPlayerModule.cpp`, `PluginEntry.cpp`, `SwiffMemory.cpp`),
`bf2_bf2audio_static` 3 (`Reverb.cpp`, `SoundFile.cpp`, `WavFileFormat.cpp`),
`bf2_plugin_kit_static` 3 (`BfMemory.cpp`, `Msvcr71Heap.cpp`, `Vc71String.cpp`). These
assertions are the first layer only: the about 3,400 `vc71::` uses counted in the
milestone 1 analysis sit behind them and behind `windows.h` in the renderer.

### Size assertions (S2.2)

S2.2 measured the Apple LP64 branches and compatibility findings. Its focused pass removes all assertion
failures from the 154 BFMeme, Menu and HUD client TUs, with 100 previously blocked
TUs now passing. The renderer and SwiffPlayer checks remain outside that package.
The S2.0 inventory below is the historical baseline.

218 distinct failing checks in 125 files, hit by 135 TUs (client objects 105, renderer
29, SwiffPlayer 1). The tree has about 1,555 `static_assert(sizeof|offsetof ...)` lines;
the rest already pass or have native branches. One failing check is an
architecture gate rather than a size: `Menu/HudBlinker.h` also asserts
`sizeof(void*) == 4` ("HUD recovery requires x86"), next to its three layout checks, and
reaches 7 TUs.

TUs that fail on a size check:

- `bf2_renddx9_static` (29): `BF2Render.cpp`, `BundledMeshMaterial.cpp`, `BundledMeshStateManager.cpp`, `BundledMeshTemplate.cpp`, `DebugLineGraphD3d.cpp`, `DebugTextWriter.cpp`, `EnvMapManager.cpp`, `EnvMapObject.cpp`, `FXParameter.cpp`, `MeshParticleMesh.cpp`, `MeshParticleMeshTemplate.cpp`, `MeshTemplate.cpp`, `ObjectDrawer.cpp`, `ObjectGeometryConsole.cpp`, `OverGrowth.cpp`, `OverGrowthCellRender.cpp`, `OverGrowthCells.cpp`, `OverGrowthConsole.cpp`, `OverGrowthLoad.cpp`, `OverGrowthRender.cpp`, `ParticleSystemTemplateType1.cpp`, `RendModule.cpp`, `RenderView.cpp`, `RoadCompiled.cpp`, `RoadCompiledRenderer.cpp`, `RoadCompiledTemplate.cpp`, `RoadEditable.cpp`, `UndergrowthSystemLoad.cpp`, `XpLightManager.cpp`
- `bf2_swiffplayer_static` (1): `SwiffPlayer.cpp`
- client `External/BFMeme/` (57): `Action.cpp`, `ActionListAction.cpp`, `ActionNode.cpp`, `AlphaFadeEffect.cpp`, `AndData.cpp`, `BlendFuncEffect.cpp`, `BoolData.cpp`, `CallFunctionAction.cpp`, `ClassIStream.cpp`, `ClassOStream.cpp`, `ColorEffect.cpp`, `CullNode.cpp`, `CullVariableActionNode.cpp`, `DynamicIndexPointer.cpp`, `Effect.cpp`, `EffectNode.cpp`, `EqualData.cpp`, `Event.cpp`, `EventQueue.cpp`, `FileIStream.cpp`, `FileOStream.cpp`, `FloatData.cpp`, `Function.cpp`, `IStream.cpp`, `InputNodeVisitors.cpp`, `IntData.cpp`, `LogicalData.cpp`, `MoveEffect.cpp`, `MovePointer.cpp`, `NameNode.cpp`, `NamedInputStream.cpp`, `NamedObjectRegistry.cpp`, `Node.cpp`, `NodeState.cpp`, `NotData.cpp`, `OStream.cpp`, `Object.cpp`, `ObjectCollection.cpp`, `ObjectList.cpp`, `OrData.cpp`, `PictureNode.cpp`, `Pointer.cpp`, `Quad.cpp`, `SetVariableAction.cpp`, `SetVariableSineAction.cpp`, `SetVariableSoftAction.cpp`, `ShowEffectNode.cpp`, `SliderNode.cpp`, `SplitNode.cpp`, `Stream.cpp`, `Style.cpp`, `System.cpp`, `TextNode.cpp`, `ToggleData.cpp`, `TransformNode.cpp`, `TypeDescriptor.cpp`, `VariableColorEffect.cpp`
- client `Game/` (1): `HudInformationLayer.cpp`
- client `Menu/` (47): `BF2HudManager.cpp`, `BF2HudManagerEvents.cpp`, `BF2HudManagerInit.cpp`, `BF2HudManagerLifecycle.cpp`, `BF2HudManagerLists.cpp`, `BF2HudManagerPointer.cpp`, `BF2HudManagerSpottedTarget.cpp`, `BF2HudManagerUpdate.cpp`, `Bf2ActionListAction.cpp`, `Bf2BarNode.cpp`, `Bf2BlendFuncEffect.cpp`, `Bf2ButtonNode.cpp`, `Bf2CallFunctionAction.cpp`, `Bf2ColorEffect.cpp`, `Bf2CompassNode.cpp`, `Bf2CullNode.cpp`, `Bf2EditNode.cpp`, `Bf2EffectNode.cpp`, `Bf2FixedSliderNode.cpp`, `Bf2HoverNode.cpp`, `Bf2HudBuilder.cpp`, `Bf2MenuMapNode.cpp`, `Bf2MiniMenuMapNode.cpp`, `Bf2MoveEffect.cpp`, `Bf2NewListBoxNode.cpp`, `Bf2ObjectMarkerNode.cpp`, `Bf2ObjectSelectionNode.cpp`, `Bf2PictureNode.cpp`, `Bf2SplitNode.cpp`, `Bf2Style.cpp`, `Bf2TextNode.cpp`, `Bf2TransformListNode.cpp`, `Bf2TransformNode.cpp`, `BfEditNode.cpp`, `BfFixedSliderNode.cpp`, `BfNewListBoxNode.cpp`, `BfOccupiedVehicleData.cpp`, `BfOccupiedVehicleNode.cpp`, `BfTransformNode.cpp`, `DifFontMetadata.cpp`, `Font.cpp`, `HudAmmo.cpp`, `HudBlinker.cpp`, `HudVehicleFrontiers.cpp`, `HudVehicleInformation.cpp`, `HudVoip.cpp`, `Ref2System.cpp`

Failing checks by file (count of checks; `[n TUs]` when a header reaches several TUs):

- `External/BFMeme/` (105 checks in 60 files): `BFMeme/PictureNode.h` 1 [9 TUs], `BFMeme/SliderNode.h` 1 [4 TUs], `BFMeme/TextNode.h` 1 [4 TUs], `BFMeme/Action.cpp` 1, `BFMeme/ActionListAction.cpp` 1, `BFMeme/ActionNode.cpp` 2, `BFMeme/AlphaFadeEffect.cpp` 1, `BFMeme/AndData.cpp` 2, `BFMeme/BlendFuncEffect.cpp` 1, `BFMeme/BoolData.cpp` 2, `BFMeme/CallFunctionAction.cpp` 1, `BFMeme/ClassIStream.cpp` 2, `BFMeme/ClassOStream.cpp` 3, `BFMeme/ColorEffect.cpp` 1, `BFMeme/CullNode.cpp` 2, `BFMeme/CullVariableActionNode.cpp` 2, `BFMeme/DynamicIndexPointer.cpp` 1, `BFMeme/Effect.cpp` 2, `BFMeme/EffectNode.cpp` 2, `BFMeme/EqualData.cpp` 2, `BFMeme/Event.cpp` 1, `BFMeme/EventQueue.cpp` 2, `BFMeme/FileIStream.cpp` 1, `BFMeme/FileOStream.cpp` 1, `BFMeme/FloatData.cpp` 2, `BFMeme/Function.cpp` 1, `BFMeme/IStream.cpp` 1, `BFMeme/InputNodeVisitors.cpp` 6, `BFMeme/IntData.cpp` 2, `BFMeme/LogicalData.cpp` 2, `BFMeme/MoveEffect.cpp` 1, `BFMeme/MovePointer.cpp` 1, `BFMeme/NameNode.cpp` 2, `BFMeme/NamedInputStream.cpp` 2, `BFMeme/NamedObjectRegistry.cpp` 4, `BFMeme/Node.cpp` 4, `BFMeme/NodeState.cpp` 1, `BFMeme/NotData.cpp` 2, `BFMeme/OStream.cpp` 1, `BFMeme/Object.cpp` 2, `BFMeme/ObjectCollection.cpp` 1, `BFMeme/ObjectList.cpp` 2, `BFMeme/OrData.cpp` 2, `BFMeme/PictureNode.cpp` 1, `BFMeme/Pointer.cpp` 1, `BFMeme/Quad.cpp` 1, `BFMeme/SetVariableAction.cpp` 2, `BFMeme/SetVariableSineAction.cpp` 2, `BFMeme/SetVariableSoftAction.cpp` 2, `BFMeme/ShowEffectNode.cpp` 2, `BFMeme/SplitNode.cpp` 2, `BFMeme/Stream.cpp` 2, `BFMeme/Style.cpp` 1, `BFMeme/System.cpp` 5, `BFMeme/TextNode.cpp` 2, `BFMeme/ToggleData.cpp` 2, `BFMeme/TransformNode.cpp` 2, `BFMeme/TypeDescriptor.cpp` 1, `BFMeme/VariableColorEffect.cpp` 1, `BFMeme/VariableColorEffect.h` 1
- `External/gameswf/` (2 checks in 1 files): `gameswf/SwiffPlayer/SwiffPlayer.cpp` 2
- `Game/` (1 checks in 1 files): `HudInformationLayer.cpp` 1
- `Geom/` (24 checks in 7 files): `MeshTemplate.h` 2 [14 TUs], `Overgrowth/OverGrowth.h` 3 [7 TUs], `BundledMeshStateManager.h` 5 [4 TUs], `BundledMeshMaterial.h` 5 [2 TUs], `BundledMeshTemplate.h` 6 [2 TUs], `ParticleSystem/MeshParticleMesh/MeshParticleMeshTemplate.h` 1 [2 TUs], `ObjectDrawer.cpp` 2
- `GeomMisc/` (2 checks in 2 files): `DebugLineGraphD3d.cpp` 1, `DebugTextWriter.cpp` 1
- `Menu/` (74 checks in 50 files): `Bf2PictureNode.h` 1 [8 TUs], `HudBlinker.h` 4 [7 TUs], `HudVoip.h` 1 [6 TUs], `BfOccupiedVehicleData.h` 1 [5 TUs], `Bf2ButtonNode.h` 1 [4 TUs], `Bf2BarNode.h` 1 [3 TUs], `Bf2ColorEffect.h` 1 [3 TUs], `Bf2EffectNode.h` 1 [3 TUs], `Bf2ObjectMarkerNode.h` 2 [3 TUs], `Bf2Style.h` 1 [3 TUs], `Bf2TextNode.h` 1 [3 TUs], `BfFixedSliderNode.h` 1 [3 TUs], `Bf2BlendFuncEffect.h` 1 [2 TUs], `Bf2CompassNode.h` 1 [2 TUs], `Bf2EditNode.h` 1 [2 TUs], `Bf2FixedSliderNode.h` 1 [2 TUs], `Bf2HoverNode.h` 1 [2 TUs], `Bf2MiniMenuMapNode.h` 1 [2 TUs], `Bf2MoveEffect.h` 1 [2 TUs], `Bf2NewListBoxNode.h` 1 [2 TUs], `Bf2ObjectSelectionNode.h` 1 [2 TUs], `Bf2TransformListNode.h` 1 [2 TUs], `BfOccupiedVehicleNode.h` 1 [2 TUs], `BF2HudManagerSpottedTarget.cpp` 1, `Bf2ActionListAction.cpp` 1, `Bf2BarNode.cpp` 2, `Bf2BlendFuncEffect.cpp` 2, `Bf2CallFunctionAction.cpp` 1, `Bf2ColorEffect.cpp` 2, `Bf2CompassNode.cpp` 2, `Bf2CullNode.cpp` 2, `Bf2EffectNode.cpp` 2, `Bf2HoverNode.cpp` 2, `Bf2HudBuilder.cpp` 1, `Bf2MenuMapNode.cpp` 1, `Bf2MoveEffect.cpp` 2, `Bf2ObjectMarkerNode.cpp` 2, `Bf2ObjectSelectionNode.cpp` 2, `Bf2PictureNode.cpp` 2, `Bf2SplitNode.cpp` 2, `Bf2Style.cpp` 4, `Bf2TextNode.cpp` 2, `Bf2TransformNode.cpp` 2, `BfEditNode.cpp` 1, `BfNewListBoxNode.cpp` 1, `BfOccupiedVehicleNode.cpp` 2, `BfTransformNode.cpp` 1, `DifFontMetadata.cpp` 1, `Font.cpp` 3, `Ref2System.cpp` 1
- `RendDX9/` (2 checks in 1 files): `FXParameter.cpp` 2
- `RendMisc/` (8 checks in 3 files): `EnvMapSystem/EnvMapManager.h` 3 [3 TUs], `EnvMapSystem/EnvMapObject.h` 4 [2 TUs], `RenderView.cpp` 1

With every assertion neutralised, all 57 BFMeme TUs, 42 of the 52 failing Menu TUs and
`Game/HudInformationLayer.cpp` compile, so for 100 client TUs the assertions are the
only compile blocker.

### Calling conventions (S2.3)

No errors. `Core/PlatformCompat.h` already maps `__cdecl`, `__stdcall`, `__fastcall`
and `__thiscall` to nothing, and every spelling the client and plugin TUs reach compiles,
including `RendDX9Abi/PluginAbi.h`'s `__fastcall` factory type and its
`NativeFactoryThunk`. What remains for S2.3 is semantic, not a compile class: the
renderer's raw thiscall/fastcall thunks behind `windows.h` (not yet reached) and an audit
of any fixed vtable offset that assumes MSVC's single destructor slot.

### Win32/DirectX API

The client rows below, the plugin rows outside the renderer and the MSVC CRT calls are resolved
on `mac/s2-58-platform`; see [Platform layer (packages S2.5 and
S2.8)](#platform-layer-packages-s25-and-s28). The S2.0 measurement follows.

Client objects (24 TUs; API used in parentheses):

| TU | Blocker | Suggested owner |
| --- | --- | --- |
| `Game/Main/BF2Window.cpp`, `Game/Main/BF2Render.cpp` | `BF2Window.h` includes `windows.h` (`HWND`, window class) | S2.4 |
| `IO/Input/DX8InputDeviceManager.cpp`, `KeyboardDevice.cpp`, `MouseDevice.cpp`, `GameControllerDevice.cpp` | DirectInput 8 (`KeyboardDevice.h`, `MouseDevice.h`, `Windows.h`) | S2.4 |
| `Menu/MainMenuLogic.cpp` | `GetAsyncKeyState`, clipboard (`OpenClipboard`, `GetClipboardData`, `GlobalAlloc`), `SetCursor`/`ShowCursor`, `HWND` | S2.4 |
| `Menu/BetweenRoundsLogic.cpp` | `SetCursor`, `ShowCursor` | S2.4 |
| `Menu/SwiffHost.cpp` | Winsock (`WSAStartup`, `gethostbyname`, `inet_addr`, `inet_ntoa`) | unowned; BSD sockets |
| `Menu/HudAmmo.cpp`, `HudInformationAdapter.cpp`, `HudVehicleInformation.cpp` | one `MessageBoxA` each | unowned, small |
| `Menu/BF2HudManagerSpotted.cpp` | `RegOpenKeyExA` | unowned, small |
| `Menu/Ref2System.cpp` | `_splitpath` | unowned, small |
| `Game/PasswordCodec.cpp`, `Game/MatchMaking/Client/CdKeyStore.cpp` | `CryptProtectData`/`CryptUnprotectData` (DPAPI) | unowned; needs a Keychain or file-format decision |
| `Game/AudioSettings.cpp` | `windows.h` (physical-memory query) | S2.5 |
| `Game/GeneralSettings.cpp` | `__time32_t`, `_time32` | unowned, small |
| `Core/StringUtil.cpp` | `_vsnwprintf`, `_swprintf` (not equivalent to `vswprintf` on truncation) | unowned, small; shared with the server's TU list |

Plugin images: `Sound/SoundEngine/Win32System.cpp` (BF2Audio's DllMain and loader
wrappers, S2.5), `RendDX9Abi/Frontier.cpp` and `RendDX9Abi/CrashReport.cpp` (kit and
renderer logging), `SwiffPlayer/SwiffCrashLog.cpp`, `SwiffPlayer.cpp`
(`sprintf_s`, `_time32`), `gameswf/gameswf_impl.cpp` (`_snprintf_s`). In the renderer, 97
TUs stop at `RendDX9/RendDX9Base.h:10` (81 TUs), `RendDX9/Texture.h:10` (9) or
`RendDX9/Ra/RaStreamDataEditable.h:10` (`d3d9.h`, 5), plus
`GeomMisc/DebugLineGraphD3d.cpp` (`_snprintf_s`). That is stage 3 work; with the
assertions neutralised, 22 renderer TUs already compile (the console-row TUs,
`MeshTemplate.cpp`, `ObjectDrawer.cpp`, `ModuleGeomMisc.cpp`, `RendModule.cpp`,
`RenderView.cpp`, `Tokenizer.cpp`, `PluginEntry.cpp` and others).

### D3DX

No errors yet: every D3DX use sits in renderer TUs behind `windows.h`/`d3d9.h`.
Stage 3.

### Bink and Novint (S2.8)

Resolved on `mac/s2-58-platform`; see [Bink, Novint and VoIP capture
(S2.8)](#bink-novint-and-voip-capture-s28). The S2.0 measurement follows.

`External/NovintHFX/NovintHFX.h:6` includes `windows.h`. It reaches 4 client TUs
through `IO/Input/FalconDevice.h`: `IO/Input/FalconDevice.cpp`,
`Game/Main/BF2Engine.cpp`, `Menu/BF2HudManagerInit.cpp` and
`Menu/BF2HudManagerUpdate.cpp`. `BF2Engine.cpp` is the largest single blocker in the
image: 18 of the 149 undefined symbols below are its members. Bink (`GeomMisc/MenuRend/Video.cpp`,
`RendDX9Abi/Bink.h`) is renderer-image code behind `windows.h`.

### Other

- Resolved by S2.7 (namespace `tu_util`, see `gameswf/GAMESWF_DIVERGENCES.md`):
  `External/base/utility.h:79, 81, 97` defines `fmax(float, float)`, `fmin(float, float)`
  and `log2(float)` at global scope; libc++'s `<math.h>` already declares those overloads,
  so 30 SwiffPlayer TUs fail. Not changed here: the gameswf versions differ from the C
  library ones for NaN and signed-zero inputs, so removing them is a behaviour decision
  for whoever owns the menu player.
- Fixed in this package (toolchain-neutral): the `PluginAbi.h` `/MD` gate (113 plugin
  TUs), `base/tu_types.h` falling back to `<SDL.h>` (35), `base/tu_math.h`'s obsolete
  OS X float macros, the console-ring declarations in `BF2.h` (2), and
  pointer-versus-`false` comparisons in `Bf2HudBuilder.cpp` (5 sites).

### Link reach

A strict link is not close yet, but its shape is measurable. Linking every object that
compiles, plus the 131 that compile with assertions neutralised (renderer excluded),
gives:

- **149 undefined symbols.** 138 are defined in the 23 client TUs and 31 SwiffPlayer
  TUs that still fail (above all `BF2Engine.cpp`, `StringUtil.cpp`, `GeneralSettings.cpp`,
  the HUD adapters, `Music.cpp`'s `win32::` wrappers, gameswf). 8 have no definition in
  a client compile on any platform (`AreaObject::handleFrameUpdate`,
  `EffectBundleTemplate::makeMemberScript`, `ObjectPriorityCalculator::update` and
  `g_nullSoundEngine` are dedicated-only bodies; `ai::dispatchArtilleryRequest`,
  `meme::MovePointer::getClassType`, `meme::getTreeClassType`,
  `meme::releaseListBoxCellResource`), and 3 have no definition at all
  (`meme::IStream/OStream/ClassIStream::getType`). Those 11 are client-wide recovery
  debt, also on the MSVC `bf2_client_linkcheck` list.
- **13 duplicate symbols** from a design issue that only static linking exposes: each
  plugin's `PluginEntry.cpp` defines its own host-service globals in `dice::hfe`
  (`classManager`, `settings`, `mainConsoleObjects`, `eventManager`, `game`, `g_system`,
  `loadStat`, `debugOn`, `debugModules`, `profilerClient`, `frameEventManager`,
  `bfGlobal_callback`, `bfGlobal_callbackData`), some with types that differ from the
  engine's. SwiffPlayer alone collides on 13; RendDX9 will add its own. In one image these
  must become the engine's globals (or move into per-image namespaces). That decision
  belongs with S2.7's plugin-boundary work.

Estimate, not measured: once S2.2 and the vc71 assertions land, a link with
`[FRONTIER]` stops for the 23 Win32/Novint client TUs and the SwiffPlayer library left
out is a few days of work: the plugin-global decision, about 140 named stubs grouped by
class, and the 11 client-wide symbols.

### Re-measurement after S2.7

Measured on 2026-10-22, branch `mac/s2-7-vc71` (S2.0 and S2.4 merged, plus this package),
same machine and toolchain, with the same per-TU driver (`-ferror-limit=0`, one log per TU)
over `compile_commands.json` of `out/build/s27-x64`. S2.1, S2.2 and S2.6 work is not on this
branch, so their classes are as S2.0 left them.

| Class | Owner | TUs, first blocker | TUs with any error | Errors | Distinct sites |
| --- | --- | ---: | ---: | ---: | ---: |
| vc71 containers | S2.7 | **0** (was 121) | 0 | 0 | 0 |
| Size assertions | S2.2 | 135 | 135 | 383 | 242 |
| Win32/DirectX API (MSVC CRT included) | S2.4 call sites, S2.5, stage 3, small items | 92 | 116 | 125 | 51 |
| Bink/Novint | S2.8 | 1 | 3 | 3 | 2 |
| Other (`base/utility.h`) | S2.7 | **0** (was 29) | 0 | 0 | 0 |
| **Total failing** | | **228** of 1,181 (was 284 of 1,178) | | | |

The two "other" errors that remain are follow-on errors in Win32 TUs (`StringUtil.cpp`'s
`_swprintf` call, `SwiffPlayer.cpp`'s `__time32_t` declaration) and are counted there. The
TU count changed because the five DirectInput TUs left the macOS manifest and the two
Objective-C++ host files joined it.

With assertions neutralised (measurement only), **119** TUs still fail (was 153): Win32/DirectX
116, of them 97 in the renderer, and Bink/Novint 3. No TU fails for any other reason.

| Group | TUs | Fail | Size | Win32/DX | Bink/Novint | Still fail |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| client `Core/` | 27 | 1 | 0 | 1 | 0 | 1 |
| client `External/BFMeme/` | 58 | 57 | 57 | 0 | 0 | 0 |
| client `Game/` | 376 | 7 | 1 | 5 | 1 | 6 |
| client `IO/` | 51 | 0 | 0 | 0 | 0 | 0 |
| client `Menu/` | 95 | 52 | 47 | 5 | 0 | 10 |
| `bf2_client` (`StaticPlugins.cpp`, two `.mm`) | 3 | 0 | 0 | 0 | 0 | 0 |
| `bf2_bf2audio_static` | 16 | 1 | 0 | 1 | 0 | 1 |
| `bf2_plugin_kit_static` | 4 | 1 | 0 | 1 | 0 | 1 |
| `bf2_renddx9_static` | 120 | 106 | 29 | 77 | 0 | 97 |
| `bf2_swiffplayer_static` | 40 | 3 | 1 | 2 | 0 | 3 |
| everything else | 391 | 0 | | | | 0 |
| **total** | 1181 | 228 | 135 | 92 | 1 | 119 |

The 25 TUs outside the renderer that still fail with assertions neutralised:

- Client: `Core/StringUtil.cpp` (`_vsnwprintf`, `_swprintf`), `Game/AudioSettings.cpp`
  (S2.5), `Game/GeneralSettings.cpp` (`__time32_t`), `Game/PasswordCodec.cpp` and
  `Game/MatchMaking/Client/CdKeyStore.cpp` (DPAPI), `Game/Main/BF2Render.cpp` (`windows.h`,
  S2.4 call sites), `Game/Main/BF2Engine.cpp` (Novint, S2.8), `Menu/BF2HudManagerInit.cpp`
  and `BF2HudManagerUpdate.cpp` (Novint), `Menu/BF2HudManagerSpotted.cpp`, `HudAmmo.cpp`,
  `HudInformationAdapter.cpp`, `HudVehicleInformation.cpp`, `Ref2System.cpp`,
  `MainMenuLogic.cpp`, `BetweenRoundsLogic.cpp`, `SwiffHost.cpp` (the Win32 table above).
- Plugins: `SwiffPlayer.cpp` (`sprintf_s`, `__time32_t`), `SwiffCrashLog.cpp` (`windows.h`),
  `gameswf_impl.cpp` (`_snprintf_s`), `RendDX9Abi/Frontier.cpp` (`windows.h`, kit),
  `Sound/SoundEngine/Win32System.cpp` (S2.5).

Changes in the size class since S2.0: `Game/Main/BF2Render.cpp` no longer stops at the
display-mode carrier checks (`RendDX9Abi/DisplayModeAbi.h`, now skipped under
`BF2_STATIC_PLUGINS`; it then stops at `windows.h`). `SwiffPlayer.cpp` is newly listed:
it got past `utility.h`, so its two layout assertions (`sizeof(SwiffPlayer) == 0x460`,
`LocalProfileRecord == 176`) are now its first blockers. These are S2.2's.

**Link reach.** A measurement link (not a build product) used every object that compiles,
plus the 100 non-renderer TUs that compile with assertions neutralised (objects outside the
build tree), with SwiffPlayer and BF2Audio prelinked as `bf2_client` does. The link uses the
same libraries and frameworks as the target, without the renderer. Results:

- **0 duplicate symbols** (with `-undefined dynamic_lookup` to get past the undefined
  phase; without the prelink the same link has 15).
- **161 undefined symbols** (S2.0: 149). Attributed by symbol name to the TU that defines
  them in source: `BF2Engine.cpp` 43 (the engine and the `g_*` service globals it defines;
  Novint, S2.8), `StringUtil.cpp` 27, the HUD TUs 24, `SwiffPlayer.cpp`/`gameswf_impl.cpp`
  18, `Win32System.cpp`'s `win32::` wrappers 11 (S2.5), `PasswordCodec`/`CdKeyStore` 9,
  `GeneralSettings.cpp` 8, `Frontier.cpp`'s `renddx9abi::log*` 4, menu logic and `SwiffHost`
  3, `BF2Render.cpp` 3, `AudioSettings.cpp` 1, and the 10 client-wide recovery-debt symbols
  listed above (`g_nullSoundEngine` is no longer referenced). The rise from 149 comes from
  the five DirectInput TUs that are no longer built and from more TUs that now compile and
  reference these owners.

By that attribution, every undefined symbol belongs to one of the 25 still-failing TUs or to
the 10 client-wide symbols. A strict link is therefore one step from the compile work: once S2.2 lands and the
small Win32 items, S2.5's `Win32System.cpp` and S2.8's Novint header are done, only the 10
client-wide symbols remain, and they need named `[FRONTIER]` bodies (known signatures) or
recovery. S2.2 is a precondition: today 100 of the linked objects exist only as
assertion-neutralised measurement objects. The renderer stays out (`BF2_MACOS_LINK_RENDDX9=OFF`).

The dedicated `bf2` was rebuilt from clean in the same build directory after these changes
and links strictly. It was not run.

### Re-measurement after S2.5 and S2.8

Measured on 2026-10-22, branch `mac/s2-58-platform` (off `mac-metal` 93eeab0, which has S2.0,
S2.4 and S2.7), same machine, toolchain and per-TU driver, over `compile_commands.json` of
`out/build/s258-x64`. S2.1, S2.2 and S2.6 work is not on this branch. The driver first reproduced
S2.7's 228 failing TUs on the unchanged tree.

| Class | Owner | TUs, first blocker | Distinct sites |
| --- | --- | ---: | ---: |
| Size assertions | S2.2 | 136 (was 135) | 243 checks, 384 errors |
| Win32/DirectX API (MSVC CRT included) | stage 3 (renderer only) | **77** (was 92) | `windows.h` and `d3d9.h` in the renderer |
| Bink/Novint | S2.8 | **0** (was 1) | 0 |
| vc71, other | — | 0 | 0 |
| **Total failing** | | **213** of 1,185 (was 228 of 1,181) | |

The TU count grew by four: `Platform/macOS/MacSystem.cpp` and `ProtectedData.cpp`,
`RendDX9Abi/BinkUnavailable.cpp`, and `IO/Input/FalconDevice.cpp`, back in the manifest. The
size class gained `Game/Main/BF2Render.cpp` and lost nothing: TUs that stopped at `windows.h`
now reach their layout checks.

With assertions neutralised (measurement only), **97** TUs still fail (was 119), **all of them
in `bf2_renddx9_static`**: 91 at `windows.h`, 5 at `d3d9.h`, and
`GeomMisc/DebugLineGraphD3d.cpp` at `_snprintf_s` (it does not include `MsvcCrtCompat.h`; it is
renderer code and left to stage 3). Outside the renderer, the only compile blocker left is S2.2.

| Group | TUs | Fail | Size | Win32/DX | Still fail |
| --- | ---: | ---: | ---: | ---: | ---: |
| client `Core/` | 27 | 0 | 0 | 0 | 0 |
| client `External/BFMeme/` | 58 | 57 | 57 | 0 | 0 |
| client `Game/` | 376 | 2 | 2 | 0 | 0 |
| client `IO/` | 52 | 0 | 0 | 0 | 0 |
| client `Menu/` | 95 | 47 | 47 | 0 | 0 |
| `bf2_client` (`StaticPlugins.cpp`, two `.mm`) | 3 | 0 | 0 | 0 | 0 |
| `bf2_bf2audio_static` | 16 | 0 | 0 | 0 | 0 |
| `bf2_plugin_kit_static` | 4 | 0 | 0 | 0 | 0 |
| `bf2_renddx9_static` | 121 | 106 | 29 | 77 | 97 |
| `bf2_swiffplayer_static` | 40 | 1 | 1 | 0 | 0 |
| everything else | 393 | 0 | | | 0 |
| **total** | 1185 | 213 | 136 | 77 | 97 |

The two client `Game/` TUs are `HudInformationLayer.cpp` and `Main/BF2Render.cpp`
(`RendererDisplayMode == 40`); the SwiffPlayer one is `SwiffPlayer.cpp` (`0x460`,
`LocalProfileRecord == 176`).

**Link reach.** The same measurement link as after S2.7 (every compiled object, the
assertion-neutralised objects of the 107 non-renderer TUs, SwiffPlayer and BF2Audio prelinked
with their image-private lists, the target's libraries and frameworks, no renderer):

- **0 duplicate symbols.** Compiling `BF2Engine.cpp` first exposed five: the
  filesystem-change ids that `IO/File/FileChangeManager.h` declared `extern` gave `Core/Ids.h`'s
  constants external linkage in each TU that included the header first. The header now uses the
  `Ids.h` constants (internal linkage, like every other id).
- **12 undefined symbols** (S2.7: 161). Every one is client-wide recovery debt with no definition
  in a client compile on any platform, so none is a platform item:
  - the ten S2.7 listed: `world::AreaObject::handleFrameUpdate`,
    `world::EffectBundleTemplate::makeMemberScript`, `world::ObjectPriorityCalculator::update`
    (dedicated-only bodies), `ai::dispatchArtilleryRequest`, `meme::MovePointer::getClassType`,
    `meme::getTreeClassType`, `meme::releaseListBoxCellResource`, and
    `meme::IStream/OStream/ClassIStream::getType`;
  - two that `BF2Engine.cpp` now references: `modInfo_vslot0x30` (`startGame`'s join check,
    IModInfo +0x30) and `unresolvedGlobalA33C08_vslot0x18` (`mainLoop`'s ragdoll-viewer test), both
    declared `[UNRESOLVED]` in `Game/Main/BF2ClientSeams.h` and defined nowhere.

  Three platform symbols surfaced on the way and were fixed: SwiffPlayer.cpp's own declarations of
  `SetCursor`, `GetKeyState` and `_time64`.

What is left before a strict client link: S2.2 (107 non-renderer TUs exist only as
measurement objects), and the twelve symbols above, which need recovery or named `[FRONTIER]`
bodies with known signatures (`modInfo_vslot0x30` and `unresolvedGlobalA33C08_vslot0x18` already
have declared signatures). The renderer stays out (`BF2_MACOS_LINK_RENDDX9=OFF`). S2.1's inline
assembly compiles on x86_64 and does not block the link, but runs with 32-bit operands.

The dedicated `bf2` was rebuilt from clean in `out/build/s258-x64` after these changes and links
strictly. It was not run.

### Client link and first startup (S2.9)

Measured on 2026-10-22, branch `mac/s2-9-link` (off `mac-metal` a93cfea: S2.0, S2.4, S2.7, S2.5/S2.8),
same machine and toolchain, build directory `out/build/s29-x64`. S2.1, S2.2 and S2.6 are not on this
branch.

**The twelve symbols.** Every recovered body was written only after its BF2.exe 1.50 instructions
had been read. REA's Ghidra import of BF2.exe did not finish within its one-hour limit, so the
instructions were read with `llvm-objdump` and a small PE data reader (both read-only, under `out/`). For the two
`world::` bodies the dedicated `bf2_w32ded.exe` was compared as well.

| Symbol | Result | BF2.exe evidence |
| --- | --- | --- |
| `world::AreaObject::handleFrameUpdate` | recovered | 0x5FF790, table 0x8F3838 +0xA8. The dedicated algorithm (observer at the origin); the bounds test is the call 0x5CD130, the same inclusive test. |
| `world::EffectBundleTemplate::makeMemberScript` | recovered | 0x6416E0, table 0x8FF850 +0xF8. Identical to the dedicated 0x5F9250 (its table 0x7E44F8 +0xF8). |
| `world::ObjectPriorityCalculator::update` | recovered (client arm) | 0x72CFD0, table 0x91C490 +0x10. The camera comes from ds:0x9A557C, the exe's "Renderer" singleton (`g_renderer`). |
| `ai::dispatchArtilleryRequest` | recovered | launchArtillery 0x7B25D8..0x7B2605: courier ds:0xA386C8 (filled from "IAIEventCourier" at 0x809121), +0x5C = `event_incomingArtillery(team, Vec2)`. |
| `meme::MovePointer::getClassType` | recovered, with the constructor | 0x82B900 (descriptor table 0x94A04C); the constructor is the factory's inlined body at 0x82B4E0. |
| `meme::IStream::getType` | recovered | 0x83A160 (table 0x94EC00, factory 0x83A1C0). |
| `meme::OStream::getType` | recovered | 0x8383E0 (table 0x94E954, factory 0x838420). |
| `meme::ClassIStream::getType` | recovered | 0x83C2B0 (table 0x94F2BC, factory 0x83C1E0 = the 0x83BFF0 constructor with a null input). |
| `modInfo_vslot0x30` | recovered | startGame 0x40544A; ModInfo table 0x8A1230 +0x30 = 0x47E590 = `getModFromPath`. |
| `unresolvedGlobalA33C08_vslot0x18` | recovered | mainLoop 0x40D227; ds:0xA33C08 is `ai::g_aiInstance` (only writer 0x801030 = `AIInstanceServer::updateInstance`), +0x18 = `isInitialized`. `unresolvedGlobalA33C08` is now a reference to `ai::g_aiInstance`. It was a separate global that nothing wrote, so the four client readers now see the AI instance. |
| `meme::getTreeClassType` | `[FRONTIER]` stop | 0x83D010 publishes `dice::meme::Tree` (factory 0x83CFF0, table 0x94F440); this tree has no Tree class. `[RETIRE-WHEN: MEME-TREE-FAMILY]` |
| `meme::releaseListBoxCellResource` | `[FRONTIER]` stop | clearCell 0x7C4CE7 / copyCell 0x7C4A5B call the list's +0x48 object at +0x20; the interface is unidentified and nothing in this tree assigns it. `[RETIRE-WHEN: LISTBOX-CELL-RESOURCE]` |

**Link.** `cmake --build out/build/s29-x64 --target bf2_client -- -k` compiles every TU except
those stopped by S2.2's size assertions and the renderer. 231 objects are missing: 134 outside the
renderer, all of which compile with `static_assert` neutralised, and 97 in `bf2_renddx9_static`,
which the target does not link. The measurement link uses the build's objects plus the 134
neutralised objects (kept in `out/s29/triage`, outside the build tree), with SwiffPlayer and
BF2Audio prelinked as the target does them. Its result:

- strict link (no `-undefined` override): **0 undefined symbols, 0 duplicate symbols**, linker
  exit 0;
- output: a Mach-O x86_64 executable that links `@rpath/libdice_py.dylib`, AppKit, QuartzCore,
  Metal, GameController, Carbon, IOKit, CoreAudio, libc++ and libSystem.

This is a measurement link, not a build product. 134 of its objects exist only with assertions
neutralised, so their size checks have not run, and the renderer is absent
(`BF2_MACOS_LINK_RENDDX9=OFF`).

**Startup reach.** The measurement binary was run for seconds only, with this setup:

- inside `sandbox-exec` with the milestone 1 offline profile, extended so that nothing under the
  home directory except the worktree's `out/` is writable;
- from APFS clones of `mods/`, `python/`, `pylib-2.3.4.zip`, `Settings/` and `Profiles/` under
  `out/s29/smoke/root`, with `HOME` pointed into `out/`;
- with no network beyond loopback, no port bound, and nothing written to the reference
  installation or by the client.

The command line was `+multi 1 +modPath mods/bf2 +fullscreen 0 +szx 800 +szy 600`.

- Production startup is reached: `main -> bf2MacMain -> BF2::init -> BF2Engine::init`, through
  `parseParameters`, `initModules`, the window and BF2Render construction, `initLateModules`, and
  into `initEngine` (debug callback, file system, log files).
- The first stop is a crash, not a `[FRONTIER]`: SIGSEGV in `initEngine -> initLocalization ->
  io::ExcelLexicon::load -> io::File::read(std::wstring&, wchar_t)`. That function reads
  `sizeof(wchar_t)` bytes per character into the shipped unchecked 4096-entry buffer. The
  localization files are UTF-16, but `wchar_t` is 4 bytes on macOS, so the 0x1B delimiter never
  matches and the buffer overflows. This is a `[PLATFORM]` item in shared IO code; its Linux arm needs
  the ELF reading. It was not fixed on this branch.
- In a measurement-only experiment (an object with a 16-bit-unit read, linked outside the build
  tree; the source was not changed), startup goes further and stops at the `Debug` assertion in
  `io::UnixFileSystem::getHomeSubstitution` ("dedicated servers aren't allowed to write to
  @HOME@"). The call chain is `initEngine -> LocalProfileManager::init -> hasDefaultProfile ->
  getLocalProfiles -> FileManager::findFiles`: the POSIX file system only has the dedicated ELF's
  `@HOME@` body, and the client needs a profile directory.
- Not reached: `initRenderer` (BF2Engine.cpp, after `LocalProfileManager::init`) and so the
  expected renderer-null failure, the main loop, the menu, the window's first frame, sound and
  input. No window was shown.
- Without `+multi 1` the client refuses to start: `SingleInstanceGuard` has no macOS arm, so
  `isFirstInstance` stays false and `GSAllowMultipleInstances` is required.

**What remains for a real `bf2_client` link and the next startup steps:**

1. S2.2: the 134 non-renderer TUs whose only compile blocker is a size assertion. Once those
   compile, `bf2_client` should link strictly as measured here; repeat the measurement to confirm.
2. Wide-character file reads on 4-byte-`wchar_t` ports (`File::read(std::wstring&, wchar_t)` and
   any other UTF-16 readers): a port decision for macOS and Linux together.
3. A client `@HOME@` substitution in `UnixFileSystem` (where profiles and logs go on macOS).
4. A macOS arm for `SingleInstanceGuard`.
5. Then startup should reach `initRenderer` and the shipped renderer-null failure. That needs
   re-measuring.
6. The two frontiers above, and the client-wide frontiers that already existed (for example the
   LocalProfileManager suffix and the haptic targets in `BF2ClientFrontiers.cpp`).

The dedicated `bf2` was rebuilt in `out/build/s29-x64` after these changes and links strictly.
It was not run.

Status when the work paused (2026-10-22): the twelve symbols are done as listed above (ten recovered,
two `[FRONTIER]` stops, none outstanding). Not measured on this branch: the coordinator reports that on
merged `mac-metal` a03bd92, `bf2_client` fails in only two TUs, `SwiffPlayer.cpp` (`sizeof(SwiffPlayer)
== 0x460`, `LocalProfileRecord == 176`) and `Game/Main/BF2Render.cpp` (`RendererDisplayMode == 40`).
Once this branch is merged there, a real strict link of `bf2_client` should be re-measured.

### Client startup (S2.11)

On `mac/s2-11-startup` from `783292a`, the normal `macos-x64` client target now
builds and links strictly: **zero unresolved link symbols**, no measurement objects
or assertion suppression. Apple clang 17, libc++, x86_64, C++14, Unix Makefiles;
RendDX9 remains absent (`BF2_MACOS_LINK_RENDDX9=OFF`).

```sh
cmake --preset macos-x64 -B out/build/s2-11-startup
cmake --build out/build/s2-11-startup --target bf2_client -j4
sh Code/Tests/macos_client_paths/build.sh
cmake --build out/build/s2-11-startup --target bf2 -j4
```

The three remaining assertions have measured Apple LP64 branches: `SwiffPlayer`
1176 bytes, legacy `LocalProfileRecord` 200, and `RendererDisplayMode` 72. Original
x86 branches remain unchanged. Profiles are saved as field-by-field `.con` text;
the legacy record is unused by the live typed profile callbacks. The display mode
is an in-memory plugin ABI, not a file format: its future native renderer producer
must validate the matching layout and accesses. No renderer exchange ran here.

The client-only `[CHOSEN, not recovered]` path policy maps `@HOME@` to
`~/Library/Application Support/OpenBF2`, honoring redirected HOME. `SingleInstanceGuard`
holds a nonblocking `flock` on `Client.lock` (since 2026-11-14 in the working directory, the
game folder; [write paths](macos-write-paths.md)), with close-on-exec
and close/process-death release. Dedicated `/tmp`/diagnostic behavior and Windows
mutex paths remain unchanged. The engine-free path/lock check passes, including
process contention and immediate reuse after close or holder death.

| Validation/reach | S2.11 result |
| --- | --- |
| Normal client compile and strict link | Pass; Mach-O x86_64, all 512 dynamic imports name their providers. |
| Dedicated `bf2` strict link | Pass; separate dedicated objects, Mach-O x86_64, zero unresolved link symbols. Native AIDLL dependency also builds/links. Not run. |
| Production startup | Not exercised. Sandbox preflight of `/usr/bin/true` failed, exit 71: `sandbox_apply: Operation not permitted`. |
| `initRenderer` and renderer-null failure | Not exercised; link success does not prove this path. |
| Socket listener / loading screen / joined player / playable client | Not exercised. No client or server was run. |

The prepared local owner runner is `out/runs/run-client.sh`, with `out/runs/offline.sb`.
It refuses to launch unless sandbox preflight succeeds, then uses APFS clones and HOME
under `out/`, the S2.9 offline profile, explicit port 16890 in cloned configurations,
`+port` and `BF2_PORT`, and no `+multi 1`. It ends its own process group after eight
seconds to close any window, retaining command/log/result files. Its refusal branch
and shell/Python syntax checks pass; the clone/run branches were not executed.

The UTF-16 reader/ExcelLexicon blocker remains S2.10's responsibility and was not
modified or bypassed. Merge reviewed `mac-metal` after that work lands before the
owner startup run; stop and report if localization still crashes. Existing profile,
SwiffPlayer, BFMeme and renderer frame frontiers remain. The full file inventory,
ABI/file-format analysis, validation limits, frontier names and handoff instructions
were recorded in the S2.11 report.

**Coordinator run (2026-10-31, after the merge into `mac-metal` d793cf1).** The owner
runner was run outside the Codex sandbox, and its sandbox preflight passed. The client
created `@HOME@` (`<run>/home/Library/Application Support/OpenBF2`) and took `Client.lock`.
After 2.0 s it stopped with SIGSEGV (`KERN_INVALID_ADDRESS` at `0x4000020`) in
`dice::hfe::io::File::read(std::wstring&, wchar_t)`: the UTF-16 reader blocker that S2.10
owns. `client.log` stayed empty. lldb cannot attach through `sandbox-exec`, so the stack
comes from the macOS crash report (`~/Library/Logs/DiagnosticReports/bf2_client-*.ips`;
it can appear a few seconds after the exit). Reach: startup is exercised up to the
instance guard; localization, `initRenderer` and the renderer-null failure are not reached.

### Renderer-free startup end (S2.12)

On `mac/s2-12-startup`, after S2.10 threads (2026-10-31). Runner:
a client runner (`run-client.sh <worktree> [build-tag]`, outside the repository), the S2.11 runner made
reusable. It also prints the crash-report stack, and `BF2_EXTRA_ARGS` passes extra
arguments.

- **Localization loads.** The S2.10 UTF-16 reader removes the `File::read(std::wstring&)`
  crash.
- **`@HOME@` writes were relative.** `FileManager::getWritePath` expands `@HOME@` and
  then calls `goUpInPath`, which drops a leading `/` (recovered behavior: Windows drive
  paths never start with one, and the Linux server's `/tmp` home was dropped the same
  way). The macOS client's absolute home therefore became a lowercased path under the
  game root. `[CHOSEN, not recovered]` fix, macOS client only: normalize only the tail
  after `@HOME@`, then prepend the home. `Profiles/Global.con` now lands in
  `~/Library/Application Support/OpenBF2/Profiles/`, and the existing Default profile is
  found there.
- **Reach: init completes, and the main loop is entered.** `BF2Engine::init` takes the
  shipped renderer-failure branch (it skips input, console and HUD setup, then goes on to
  network initialization). The first main-loop frame then stops with SIGSEGV in
  `BF2Engine::checkMovie()`. `createInstance("dice.hfe.geom.FSMoviePlayer")` returns null,
  because that class lives in the renderer image, and the shipped code calls `load` on it
  unguarded. That is shipped behavior with a renderer image missing, so it is not patched.
  `+skipMovies 1` on the command line does not reach `settings->boolGet("skipMovies")`.
- **Consequence for the plan:** this is the end of renderer-free startup (gate G0).
  Window, input and HUD initialization sit behind a successful `initRenderer`, so they are
  validated together with the first Metal frame (G3), not before it.

### Status and next steps (2026-10-31)

Merged in `mac-metal` (8f74c0d):
- S1a (universal build), S2.0, S2.1, S2.2, S2.4, S2.5/S2.8, S2.6 (native AI image),
  S2.7, S2.9 (client links) and S2.11 (client paths and instance guard).
- S2.10 threads (UTF-16 reader, POSIX atomics) and S2.10 numerics (x87 helpers, arm64
  extended arithmetic).
- S2.12: absolute `@HOME@` writes; renderer-free startup reaches the main loop.
- PR #55 (native STL instead of the vc71 containers; merged with `e8f25fc` as the second
  parent).

Checked after the last merge: `bf2_client`, `bf2` and `bf2_aidll` link, and the client
run reaches the main loop (S2.12). Renderer: 20 of 121 `bf2_renddx9_static` TUs compile.
99 of the failures are `windows.h`/`d3d9.h` not found, so the rest is device-seam work.
In flight: `r0-d3d9-inventory` (Codex).

**Effect compilation policy (owner, 2026-11-01), [CHOSEN, not recovered]:** the macOS client
has no Wine dependency at runtime, install time or build time. Native compilation is the only
route: vkd3d-shader + our D3D9 asm assembler + `.fx` parser + MojoShader to MSL. Results live in `@HOME@/FxNative`
(`BF2_FX_NATIVE_CACHE` overrides). `BF2_FX_CACHE`, the Wine-built FxCache reader,
`identity.json` and its missing-identity startup stop have been removed from the client.
Missing native dependencies fail CMake configuration with build instructions; there is no
cache-only client build. See [building](building.md#macos) for the one-command build.

The Wine/D3DX pipeline remains a **developer oracle, not used by the client**. Its tools
are optional and outside the client build; the checked-in refusal table contains metadata
and is converted to C++ without invoking Wine or regenerating measurements.
Effects that cannot compile natively retain the named refusal or `[FRONTIER]` stop;
there is no silent fallback. Native output is validated against the Wine/D3DX reference
only in optional developer work.
No prebuilt cache ships (it derives from the game's shaders).

Remaining work for a client that runs on macOS:

1. **Renderer-free startup: done (G0).** See S2.12 above. Window, input and HUD start
   only behind a working renderer and are checked at the first Metal frame.
2. **M2 (Apple silicon) validation, owner-run.** The full renderer-ON arm64 client, server,
   AI image and tests now cross-build and link ([c-arm64-client](macos-c-arm64-client.md));
   its bundle `out/m2/c-arm64-client/run.sh` adds the Metal, fx and client-start checks. Run the stage 1a bundle, the S2.10
   threads TSan runner (`s2-10-threads/out/m2/tsan/run-tsan.sh`) and the numerics bundle
   (`s2-10-numerics/out/m2/numerics/run.sh`) on the M2.
3. **Device-seam inventory** (in flight). Count every D3D9 and D3DX call the renderer
   makes, by kind (resources, lock/unlock, render targets, state, vertex declarations,
   draws, queries). The seam interface is designed from that list. It needs a D3D9 backend
   that wraps today's calls 1:1, so that it can be checked on Windows first (owner run).
4. **The renderer image on LP64.** With the seam headers replacing `windows.h`/`d3d9.h`
   in the 101 failing TUs, a `bf2_renddx9_static` that compiles and links. Inline
   assembly and size assertions inside the image are handled along the way.
5. **Shader tool chain (approved by the owner).** Install an HLSL-to-MSL tool chain,
   translate one simple effect, compare it with a hand port, then decide how the other
   100 effects are done.
7. **Metal device and effect framework.** Resources, state, draws, techniques, passes and
   parameter binding. DDS DXT1/3/5 maps to BC1-BC3. Replace the D3DX texture, mesh and math
   helpers.
8. **First pixels, in this order:** clear and present, the menu through `ISwiffRender`,
   the HUD, then a first map (terrain and static meshes first, then the rest).
9. **Loopback play.** The client joins a sandboxed local server on port 16890, with bots.

### Suggested order (S2 planning, historical)

Kept for reference. Items 1-6 are done; the current list is above.

1. **S2.2 first**: the 100 client TUs whose only blocker is an assertion, most of them
   in `External/BFMeme/` and the `Menu/Bf2*` node family. Start with the shared headers
   (`BFMeme/PictureNode.h` [9 TUs], `Menu/Bf2PictureNode.h` [8], `Menu/HudBlinker.h` [7,
   including the `sizeof(void*) == 4` gate], `Menu/HudVoip.h` [6],
   `Menu/BfOccupiedVehicleData.h` [5], `BFMeme/SliderNode.h`, `BFMeme/TextNode.h`,
   `Menu/Bf2ButtonNode.h` [4 each]), then the per-TU checks. The renderer's
   `Geom/MeshTemplate.h` [14] and `Geom/Overgrowth/OverGrowth.h` [7] can wait for stage 3.
2. **S2.1 in parallel**: the 25 TUs listed above, BFMeme and Menu first, because they
   compile on x86_64 today and would run with truncated pointers.
3. **S2.4**: `BF2Window`, the DirectInput devices, `MainMenuLogic`, `BetweenRoundsLogic`
   (see the hook list below). **S2.8**: `NovintHFX.h`, which unblocks `BF2Engine.cpp` (done on
   `mac/s2-58-platform`).
4. **S2.7** (done on `mac/s2-7-vc71`): decide what `Vc71Vector.h`/`Vc71Map.h`/`Vc71String.cpp` mean in a native
   64-bit image (under `BF2_PLUGIN_ABI_NATIVE` the host already uses native containers),
   together with the duplicated plugin globals. This unblocks SwiffPlayer and BF2Audio
   compiles beyond their first layer.
5. **S2.5**: `Win32System.cpp`, `AudioSettings.cpp` (done on `mac/s2-58-platform`).
6. The small unowned items in the Win32 table (done on `mac/s2-58-platform`, DPAPI included),
   then a `[FRONTIER]`-stub link for the twelve client-wide symbols once S2.2 lands. The twelve
   symbols are done on `mac/s2-9-link`: ten recovered, two `[FRONTIER]` stops. See the S2.9 section.
7. **S2.3** has nothing to compile-fix; fold it into the renderer work or S2.7.

### Where S2.4 hooks in

- `Game/Main/BF2.cpp`: on macOS the client takes the non-Win32 `main` at the end of the
  file (the ELF's dedicated entry, `#else` of the `_WIN32` block), so there is no
  message pump. `WinMain`/`bf2WinMain`, the window procedure and the menu key ring
  (`menuKeyBuffer`, `pushConsoleChar`, `pushMenuKey`) are Win32-only.
- `Game/Main/BF2.h`: `consoleCharBuffer`/`consoleCharCount`/`Read`/`Write` are now declared
  for every client (defined in `BF2Console.cpp`); the menu ring is still Win32-only, and
  `Menu/MainMenuLogic.cpp` reads it.
- `Game/Main/BF2Window.h/.cpp`: the `Window`/`BF2Window` classes (virtual slots +0x00..),
  the S2.4 window host's seam.
- `Game/Main/BF2Render.cpp`: creates the window and registers the `hWndBf2TempSlt`
  carrier before loading the renderer (`RendDX9Abi/DisplayModeAbi.h`).
- `Game/Main/BF2Engine.cpp`: Win32 calls and the Falcon global.
- `Core/ModuleRegistry.cpp`, `io::InputDX8Module::getClassList`: the client registers
  `DX8InputDeviceManager` here; a macOS device manager replaces that row.
- `IO/Input/KeyboardDevice`, `MouseDevice`, `GameControllerDevice`: the `DIK_` mapping
  targets.
- `Menu/MainMenuLogic.cpp`, `Menu/BetweenRoundsLogic.cpp`: cursor, clipboard and
  `GetAsyncKeyState`.
- `CMakeLists.txt`, the macOS `bf2_client` block: add the Cocoa sources and frameworks
  there.

## Static plugins on native ports (package S2.7)

Decisions for linking the renderer, SwiffPlayer and BF2Audio images statically into the
64-bit macOS client. All of them are `[CHOSEN, not recovered]` port policy; the MSVC x86
build keeps its layouts and DLL behaviour.

### vc71 on native ports

**Decision.** In a native 64-bit image the `vc71::` containers are plugin-private storage,
not a host ABI. They keep their member-based implementations at native pointer width; their
VC7.1 x86 size checks apply only to 32-bit builds; and the tree helpers that address a node
through a caller-supplied x86 node size stop with a `[FRONTIER]` instead of writing through
x86 offsets. The host boundary is native STL, as issue #7 directs.

`Code/RendDX9Abi/Vc71Layout.h` defines `BF2_VC71_RETAIL_LAYOUT` (1 for `_M_IX86` or
`__i386__`, else 0), and every vc71 header uses it:

| Item | 32-bit builds (MSVC x86) | Native 64-bit ports |
| --- | --- | --- |
| `vc71::vector_layout`, `vectorRef`, `vectorReserveIn`/`vectorPushBackIn` | 16 bytes, asserted | four pointer words, asserted; same code |
| `vc71::string` / `wstring` | 28 / 28 bytes, asserted | 32 / 48 bytes (LP64, 4-byte `wchar_t`), asserted; same code, `BUF_SIZE` unchanged |
| `map_node` value | `char[32]`: 28-byte key, value at +28 | `char[sizeof(vc71::string) + sizeof(void*)]`, value at `MAP_NODE_VALUE_OFFSET` (= `sizeof(vc71::string)`, 28 on x86) |
| `set_node::key` (`set_key`) | `unsigned int` | `uintptr_t`: `setEraseValue` stores pointers in it |
| `map_node`, `map_layout`, `set_node`, `set_layout`, `map_node_u32` size checks | asserted as before | not asserted (no retail meaning) |
| `map_layout::construct(nodeSize)`, `insertMultiFloat`, `mapInsert`, `mapInsertInsensitive` | unchanged | `vc71::nativeLayoutFrontier`: logs `[FRONTIER] <name> reached with VC7.1 x86 node size 0x..`, flushes stderr, aborts. `[RETIRE-WHEN: vc71-native-trees]` |
| `Vc71IntMap.h` | already `_M_IX86`-gated | unchanged |

Reasons:

- **There is no boundary left to preserve.** Under `BF2_PLUGIN_ABI_NATIVE` the host already
  hands plugins `std::string`, `std::list` and `std::vector` (`HostString`,
  `HostVectorWriter`, `RD9_HOST_VECTOR_PARAMETER` in `PluginAbi.h`). Static linking removes
  even the DLL boundary. No host code reads a `vc71::` object, so its byte layout is a
  private matter of the code that owns it.
- **The member-based containers are width-neutral.** `vc71::string`, the vector view and the
  tree walkers over the node structs use members, not byte offsets, and work unchanged at
  64-bit width. The SwiffPlayer and BF2Audio code the macOS client links uses only these
  (`vc71::string`, `wstring`, `vectorRef` for the queued-mouse vector).
- **The raw-size helpers cannot be made correct locally.** Their callers pass x86 node sizes
  (`0x30`, `0x18`, `0x14`, `0x48`, `0x830`) and the helpers write colour and nil flags at
  `nodeSize - 4`/`- 3` and keys at `+0x0C`. They live in renderer TUs that also place the
  trees at fixed x86 offsets (`m + 0x1BC`). Translating them means changing every caller's
  layout; that is the renderer migration to native containers (macOSplan.md S2.7 long tail,
  together with stage 3). Until then a native build that reaches one stops visibly. The
  renderer is not linked on macOS (`BF2_MACOS_LINK_RENDDX9=OFF`), so the client cannot reach
  them today.
- **Replacing `vc71::` with `std::` aliases now was rejected.** About 3,400 references, many of
  them byte-offset consumers, would change at once without a renderer that can run to test
  them.
- **MSVC x86 is unchanged.** On `_M_IX86` every type, size and offset is the same as before,
  including `set_key` (`unsigned int`) and `MAP_NODE_VALUE_OFFSET` (28). A measurement TU
  compiled for `i386` and `x86_64` checks both sets of sizes; see the S2.7 measurement below.

Not covered: renderer TUs still stop at `windows.h`/`d3d9.h`, so vc71 consumers behind those
includes (for example pointer-to-`unsigned int` casts in renderer code) are not yet measured.

### Plugin globals

**Decision.** Each statically linked plugin image keeps its own copies of the host-service
globals, as its DLL did, and the link makes those copies local to the image. The plugin
copies do not become references to the host's.

Each image's `PluginEntry.cpp` (`SwiffPlayerAbi/`, `RendDX9Abi/`) defines twenty
`dice::hfe` globals (`classManager`, `settings`, `mainConsoleObjects`, `mainConsole`,
`fileManager`, `fileChangeManager`, `bfGlobal_callback`, `bfGlobal_callbackData`, `debugOn`,
`debugModules`, `profilerClient`, `g_system`, `loadStat`, `eventManager`,
`frameEventManager`, `objectManager`, `objectTemplateManager`, `playerManager`, `game`,
`aiConnection`), and the image also defines `dllmodules` and `getNoModules`. Its `initDll`
fills the globals from `IClassManager::getSingleton` in shipped order and returns false when
one of the sixteen required singletons is missing. The host defines thirteen of the same
names in `Core/Globals.cpp`, `Debug.cpp` and `ConsoleObjects.cpp`, with the host's types
(`ClassManager*`, `SettingsRepostitory*`, `bool*`, `DebugCallback*`, `IGame*` and so on).

On macOS, `bf2_prelink_static_plugin` in `CMakeLists.txt` prelinks each plugin's static
library into one relocatable object:

```
clang++ [-arch ...] -r -nostdlib -o <lib>_prelinked.o -Wl,-force_load,<lib>.a \
        -Wl,-unexported_symbols_list,Code/StaticPlugins/image-private-symbols.txt
```

`ld -r` binds every reference inside the image to the image's own definitions, then the
listed symbols lose their external linkage. The host's globals keep theirs. All other
symbols keep their linkage, so inline functions and template instances still merge with the
host's copies. `ld -r` also makes symbols that were already hidden (libc++'s
`_LIBCPP_HIDE_FROM_ABI` helpers) local. They were never visible outside a linkage unit, so
this changes nothing. `bf2_client` links the prelinked SwiffPlayer object, and the
RendDX9 object when `BF2_MACOS_LINK_RENDDX9=ON`.

The same list also makes RendDX9's copy of the console class defaults private: the
`SetConsoleClassDefaults` constructor and the `consoleDefault*` accessors that
`RendDX9Abi/NativeConsoleObjects.cpp` defines for the image (RendDX9 PE 0x1007BF60). The
renderer's binding constructors then read the renderer's defaults, and the host's bindings
read `Core/ConsoleObjectBase.cpp`'s, as with two DLLs. SwiffPlayer defines none of these
names, so its references still bind to the host's.

BF2Audio defines none of the host-service globals, but it has its own null sound pair:
`g_nullSound` and `g_nullSoundTemplate`. `getSoundEngine` creates them in
`Sound/SoundEngine/BfSoundEngine.cpp`, and the image's copy of `Sound/NullSound.cpp`
(compiled with `BF2_IMAGE_BF2AUDIO`) returns them from `NullSoundTemplate::createSound` and
`NullSound::getTemplate`. The host defines the same names, which gave two more duplicates
once the image was linked whole. BF2Audio is therefore prelinked too, with
`Code/StaticPlugins/bf2audio-private-symbols.txt`: the pair, plus the `NullSound` and
`NullSoundTemplate` members, vtables and type info. The `ISound`/`ISoundTemplate` interface
type info stays shared.

Reasons:

- **The `initDll` contract is unchanged.** Every image still pulls its nineteen singletons
  in order, checks the required sixteen and returns the same result. No source changes.
- **Sharing the host's globals would be wrong.** The types differ (`IBase*` in the plugin,
  `bool*` or `SettingsRepostitory*` in the host). The plugin stores `getSingleton`'s
  unadjusted `IBase*`, and that store would overwrite the host's typed pointer with a value
  of another type and without the host's base adjustment. Per-image copies are what the
  retail DLLs had.
- **Per-image C++ namespaces or asm labels were rejected.** Plugin TUs declare these names
  through host headers too (`Core/System.h`, `EventManager.h` and more renderer-local
  `extern`s). An inline namespace makes unqualified lookup ambiguous. Clang rejects an asm
  label that comes after the first use, so every host header would need a per-image macro.
  The link-level answer needs no source change and no MSVC or Linux change.
- **Checked.** In the prelinked SwiffPlayer object the 22 listed symbols are `non-external`,
  and none of its 443 weak text symbols references one of them. In the BF2Audio object, none
  of its 108 weak text symbols references one of its own listed symbols. So no merged inline
  copy can read the wrong image's global. A measurement link of the client (see below) has no
  duplicate symbols. Without the prelink it has 15: the 13 that S2.0 found and the null sound pair. Rerun this check (an `objdump -dr` scan of the prelinked object)
  when a plugin starts to use host inline code that touches these names.

Windows is unchanged: the DLLs keep their own globals by construction, and the prelink step
exists only in the macOS block.

## Platform layer (packages S2.5 and S2.8)

Branch `mac/s2-58-platform`. Every source arm described here is `[CHOSEN, not recovered]`
and sits behind `!defined(_WIN32)`, `__APPLE__` or `!defined(_MSC_VER)`; the Win32 text of
each file is unchanged. MSVC and Linux were not built (no toolchain on this machine).

### Bink, Novint and VoIP capture (S2.8)

Current macOS movie policy supersedes S2.8's `Video::open` and `Video::initSound` aborts:
P2, Bink unavailable follows the coordinator-approved
x64 Windows precedent from [issue #7](https://github.com/B2BF/OpenBF2/issues/7).
`Video::open` logs `movie skipped` once per path and returns false; `initSound`
retains the x64 true result, `doFrame` has no frame to decode, and `isDone` is true.
Callers retain their existing failure paths. `[CHOSEN, not recovered]`,
`[RETIRE-WHEN: macos-bink-decoder]`: retired 2026-11-01 for macOS by the vendored FFmpeg
decoder (`Code/BF2/External/ffmpeg-bink/README.md`); the policy remains for Windows x64. The unreachable low-level imports retain their named
aborting frontiers; other POSIX and Windows paths are unchanged.

| Runtime | macOS behaviour | Where |
| --- | --- | --- |
| Novint HFX (Falcon) | Archived on every platform (2026-11-15, issue #17): the device, the SDK header and the force-effect code moved to `Code/Archive/NovintFalcon`, and the call sites take retail's no-Falcon path. `MacInputDeviceManager` reports `IDFFalcon` as not opened. | `Code/Archive/NovintFalcon/README.md` |
| PunkBuster client | Not installed (game-frontier batch 16). The client glue (`Game/PunkBuster/PbClient.cpp`) loads `pbcl.dll` dynamically on Windows, as retail does. Outside Windows its library seam has no loader: joining a PunkBuster server shows "PunkBuster Client Not Installed" once and turns `cl_punkbuster` off, and no file in the game's `pb` folder is touched. `bf2_punkbuster_client_check` drives the glue against a fake DLL. | `Game/PunkBuster/`, `CMakeLists.txt` |
| PunkBuster server | Not installed (game batch 19). The server glue (`Game/PunkBuster/PbServer.cpp`) loads `pbsv.dll` dynamically on Windows, as retail does. Outside Windows its library seam has no loader: engine init takes the not-installed path (its notice is dropped, because no game exists yet, as in retail), `sv.punkBuster` is switched off, `isPbSvEnabled` answers false, and no file in the `pb` folder is touched. `bf2_punkbuster_server_check` drives the glue against a fake pbsv. | `Game/PunkBuster/`, `CMakeLists.txt` |
| Bink | Played (2026-11-01, `Code/BF2/External/ffmpeg-bink/README.md`). `RendDX9Abi/BinkFFmpeg.cpp` implements the ten `binkw32` entry points on FFmpeg 7.1.1's Bink video/audio decoders, vendored unmodified in `External/ffmpeg-bink` (LGPL-2.1+) over an OpenBF2 shim; `Video.cpp` takes the x86 arms (`BF2_VIDEO_BINK`). Audio goes to an AudioQueue after `BinkSetSoundSystem`. The refusal policy above is retired for macOS; Windows x64 keeps it. | `GeomMisc/MenuRend/Video.cpp`, `RendDX9Abi/BinkFFmpeg.cpp`, `External/ffmpeg-bink/` |
| VoIP capture | Superseded 2026-11-13 (owner: rebuild the voice client in full). `BF2VoipClient` is recovered (`Game/VoipClient/`, BF2.exe 0x4510F0..0x45235C and its base 0x736C30..0x737D4E) on GameSpy Voice 2 with Speex 1.0.5 (`External/speex-1.0.5`). Windows captures through DirectSound and sets the microphone through winmm, as retail. macOS uses the SDK's CoreAudio layer (`gvOSXAudio.c`) and a CoreAudio mixer backend (`VoipMixerCoreAudio.cpp`: input volume; no boost or mute control, so retail's no-control branches). `BF2_VOIP_CAPTURE=fake` selects a synthetic device and no mixer: tests and loopback gates never touch the microphone and never trigger the permission prompt. The app bundle's `NSMicrophoneUsageDescription` belongs to the setup assistant (openbf2-setup). | `[CHOSEN, not recovered]` test device only |

Historical S2.8 reach: the renderer was excluded (`BF2_MACOS_LINK_RENDDX9=OFF`),
so its Bink stops were not exercised then. P2 supersedes those macOS stops; its
report recorded the caller checks and focused tests.
No client movie, menu or loading-screen runtime result is claimed for P2.

### Audio (S2.5)

BF2Audio's engine already speaks OpenAL through 39 entry points resolved by name
(`Sound/SoundEngine/BfOpenAL.cpp`). The POSIX arm of `Sound/SoundEngine/Win32System.cpp`
gives the eleven `win32::` wrappers native bodies:

| Wrapper | Win32 | POSIX/macOS |
| --- | --- | --- |
| `loadLibrary`, `freeLibrary`, `getProcAddress` | `LoadLibraryA`, `FreeLibrary`, `GetProcAddress` | `dlopen(RTLD_NOW \| RTLD_LOCAL)`, `dlclose`, `dlsym`; `OpenAL32.dll` and `BF2OpenAL.dll` (case-insensitive) map to `/System/Library/Frameworks/OpenAL.framework/OpenAL` |
| `outputDebugString` | `OutputDebugStringA` | `stderr` (the client replaces the default trace in `BfSound::init`) |
| `sleep`, `exchange` | `Sleep`, `InterlockedExchange` | `nanosleep` (restarted on `EINTR`), `__atomic_exchange_n` |
| `createThread`, `joinAndDeleteThread`, `threadShouldStop`, `threadArgument` | `CreateThread` at `THREAD_PRIORITY_TIME_CRITICAL`, interlocked stop flag, `WaitForSingleObject` | `pthread_create`, `QOS_CLASS_USER_INTERACTIVE`, atomic stop flag, `pthread_join`; `Thread::id` is the Mach thread id |
| `hasSoundDevice` | `DirectSoundEnumerateA`, the first named device | CoreAudio's default output device; the same "Found sound device %s (%s)" info line (module `CoreAudio`) and the same warning on a failed query |

Measured on this machine: Apple's OpenAL opens the default output for every device name the
seven provider rows pass (`DirectSound`, `DirectSound3D`, `MMSYSTEM`, `Default Device`, null),
so no device-name mapping is needed. It has no EAX, so the "System OpenAL" rows are refused
unless `allow-any-system-openal` is set, as on a Windows machine without Creative's OpenAL; the
default "Software Audio" rows load `BF2OpenAL.dll`, which maps to the same framework.

One Apple deviation needed a port arm: OpenAL.framework keeps buffers that were queued on a
source attached after they are unqueued, so the music thread's `alDeleteBuffers` failed with
`AL_INVALID_OPERATION` and leaked twelve buffers per stop. On Apple the thread clears the
source's `AL_BUFFER` first (`Music.cpp`); for spec-conforming implementations that is a no-op.

`Game/AudioSettings.cpp`'s physical-memory query emulates `GlobalMemoryStatus` for the
large-address-aware 32-bit `BF2.exe` (its PE header has the flag): `hw.memsize` saturated to a
32-bit `DWORD`, then the shipped `(dwTotalPhys + 0x7FFFF) >> 20` arithmetic in 32 bits. With
4 GB or more that wraps to 0 MB, so the default sound quality is "Low", as the retail client
picks on such a machine.

`bf2_client` links CoreAudio and CoreFoundation; OpenAL.framework is loaded at run time, as the
Win32 image loads `OpenAL32.dll`. OpenAL Soft was not needed and not installed.

Test: `Code/Tests/macos_audio` (`[TEMP]`, `[RETIRE-WHEN: macos-client-audio]`) links the
BF2Audio, Vorbis and kit static libraries and drives `getSoundEngine()`: `initialize` with
provider code 6 ("Software Audio"), master volume 0.25, one 2D play of
`Sound/Menu/front_end/menuButtonOpen.wav`, then three seconds of
`Sound/Menu/music/menu_music.ogg` through the music thread, `stopMusic`, `shutdown`. Both files
are copies from the Wine bundle's `Common_client.zip` under `out/s258-audio/`. Result: device
"MacBook Pro Speakers (CoreAudio)", 128 voices, EAX 0, the menu sound playing, the music thread
started and stopped, no OpenAL error after the `Music.cpp` arm (before it, the one
`alDeleteBuffers` error above).

```sh
cmake --build out/build/s258-x64 --target bf2_bf2audio_static bf2_vorbis_static bf2_plugin_kit_static
Code/Tests/macos_audio/build.sh out/build/s258-x64
cd out/s258-audio && ../build/s258-x64/macos_audio/bf2audio_play menuButtonOpen.wav menu_music.ogg 3
```

Not exercised: the client's own `BfSound` front end (the client does not link yet), 3D
positioning, reverb, streaming ambients, device changes while running, and arm64.

### Win32 and CRT items (S2.5/S2.8)

`Code/BF2/Core/MsvcCrtCompat.h` (empty under `_MSC_VER`, included only where used) rebuilds
the MSVC CRT functions the client calls, with MSVC's observable results. `Code/Tests/macos_crt`
(`[TEMP]`) checks them.

| Function | Semantics kept | Users |
| --- | --- | --- |
| `_vsnwprintf` | MSVC 7.1: a shorter result is terminated and its length returned; exactly `count` characters are written unterminated and `count` returned; a longer result writes `count` characters, unterminated, and returns -1. MSVC's wide conversions are rewritten for the C library: `%s`/`%c` take wide arguments, `%S`/`%C`/`%hs`/`%hc` narrow, `%ws`/`%ls` wide, `I64` → `ll`, `I` → `z`. | `StringUtil.cpp` (`formatString(const wchar_t*)`) |
| `_swprintf` | The legacy unbounded form, terminated, length returned. | `StringUtil.cpp` |
| `_snprintf_s`, `_vsnprintf_s`, `_TRUNCATE` | Truncate to the buffer (or `count`), terminate, -1 when cut. | `gameswf_impl.cpp`, `RendDX9Abi/Frontier.cpp` |
| `sprintf_s` (array form) | Formats with the terminator; an overflow reports and aborts, as the default invalid-parameter handler ends the process. | `SwiffPlayer.cpp` |
| `__time32_t`, `_time32` | 32-bit seconds; -1 once the time no longer fits (2038). | `GeneralSettings.cpp`, `SwiffPlayer.cpp` |
| `_splitpath` | MSVC `splitpath.c`: a drive when the second character is `:`, the directory up to the last `/` or `\`, the extension from the last `.` after it, MSVC part limits (`_MAX_DIR`, `_MAX_FNAME`, `_MAX_EXT` 256). | `Menu/Ref2System.cpp` |

Win32 call sites, all `[CHOSEN, not recovered]`:

| Site | Win32 | macOS |
| --- | --- | --- |
| `BF2Engine.cpp` mainLoop | `ShowCursor` | `CocoaHost::showCursor` (same counter) |
| `BF2Engine::browseModHomepage` | `ShowWindow(SW_MINIMIZE)`, `ShellExecuteA(url)` | `CocoaHost::minimize`, `openURL` |
| `BF2Engine::restartBF2` (and `activateMod`) | `GetModuleFileNameA` + `ShellExecuteA(exe, args)` | `macos::relaunchExecutable`: `_NSGetExecutablePath`, the command line split as `CommandLineToArgvW` splits it, `posix_spawn` in the current directory and environment (`Platform/macOS/MacSystem`) |
| `HudAmmo.cpp`, `HudInformationAdapter.cpp`, `HudVehicleInformation.cpp` | `MessageBoxA` | `macos::showMessageBox`: a blocking `CFUserNotificationDisplayAlert` with one OK button, Windows-1252 text |
| `BF2HudManagerSpotted.cpp` | `RegOpenKeyExA` (the Battlefield 2142 install probe) | key presence in `Registry.con`; see [macOS write paths](macos-write-paths.md#registrycon-syntax) |
| `SwiffHost.cpp` | Winsock 1.1 (`WSAStartup`/`WSACleanup`, `gethostbyname`, `inet_addr`, `inet_ntoa`) | BSD sockets, no startup call. The address word is `uint32_t` (Winsock's 32-bit `u_long`): an LP64 `unsigned long` would read eight bytes from the four-byte `h_addr_list` entry. |
| `SwiffHost::getCursor` | `LoadCursorFromFileA`/`LoadImageA` of three `.ani` files | host cursor tokens (`CocoaHost::cursorHandle`): arrow, busy, arrow for the keybinding cursor; null, as the Win32 load answers, when the file cannot be read. AppKit cannot load `.ani`, and has no public busy cursor (macos-s2-4.md). |
| `SwiffHost::setBusy` | `UpdateWindow` | true while a window exists (`UpdateWindow` fails only without one) |
| `MainMenuLogic.cpp`, `BetweenRoundsLogic.cpp` | `ShowCursor`, `SetCursor` | `CocoaHost::showCursor`, `CocoaHost::setCursor` (`SetCursor` semantics: null hides the cursor over the view) |
| `MainMenuLogic::handleClipboardShortcut` | `GetAsyncKeyState`, `CF_TEXT` clipboard | host modifiers (Control or Command as Ctrl, Option as Alt), `get/setClipboardText` (Windows-1252) |
| `BetweenRoundsLogic.cpp menuScanCodeToChar` | `ToAsciiEx(MapVirtualKeyEx(scan))` with `GetKeyboardState` | `macKeyCodeFromDik` + `keyCharacter(keyCode, modifiers)`, now Shift- and Caps-Lock-aware; one character up to 0xFF or none |
| `Game/Main/BF2Render.cpp` | `GetCurrentProcessId` in the window title | `getpid` |
| `RendDX9Abi/Frontier.cpp` (kit) | critical section, `OutputDebugStringA`, `GetLocalTime`, `GetEnvironmentVariableA`, `IsDebuggerPresent` | recursive pthread mutex, stderr, `gettimeofday`/`localtime_r`, `getenv` with `GetEnvironmentVariableA`'s too-small answer, `P_TRACED`; the log text is unchanged |
| `SwiffPlayer/SwiffCrashLog.cpp` | vectored exception logger (`[TEMP]`, opt-in) | not available; with `BF2_SWIFFPLAYER_CRASHLOG` set it logs once that nothing was installed |

### Protected profile data (DPAPI)

**Owner decision.** The client protects two secrets with DPAPI (`CryptProtectData` with
`CRYPTPROTECT_UI_FORBIDDEN`): the saved profile password, written to `Profile.con` as
`LocalProfile.setPassword "<lowercase hex of the blob>"` (`Game/PasswordCodec.cpp`), and the CD
key, kept as `x9392` + hex of the blob (`Game/MatchMaking/Client/CdKeyStore.cpp`). That text form
is the file-format contract. A DPAPI blob is bound to the Windows user account that made it:
it never decrypts on another machine or account, so no macOS approach can read a password
or CD key that Windows stored, and a migration would have to run on the Windows side. In both
directions the shipped decode-failure path applies: `decode` returns false and the profile keeps
the hex text as its password (`LocalProfileManager::setPassword`), and `translateCdKey` passes a
marked payload through verbatim.

Options:

| Option | Protection | Cost |
| --- | --- | --- |
| A. Keychain item per secret; `Profile.con` holds a reference token | Strongest at rest (login-keychain encryption); nothing secret in the profile | Changes what the profile field means on macOS; ACL prompts ("bf2_client wants to use your confidential information") for every unsigned or re-signed build; profile copied to another Mac carries a dangling token |
| B. Keychain-held master key; the profile holds the encrypted blob as today | Same as A at rest; the profile keeps carrying the protected data, as DPAPI does | Same ACL prompts for development builds; a locked keychain (login window, SSH) makes protect and decode fail |
| C. File-held master key; the profile holds the encrypted blob | The key file is readable only by the user (0600 in a 0700 directory) and lives outside the profile tree, so a copied or shared profile does not reveal the secret; at rest it relies on FileVault | Any process running as the user can read the key, which is also true of DPAPI (any such process can call `CryptUnprotectData`) |
| D. Plain hex, no protection | None | Not acceptable |

**Recommendation and implementation: C.** It keeps DPAPI's useful property (a profile copied off
the machine or posted for a bug report does not leak the password), keeps the Windows format
and code unchanged, works headless and without prompts, and is testable without touching the
user's keychain. The blob carries a version byte, so moving the key into the Keychain (option B)
later is a deliberate migration: decode with the file key, re-encode with the new one.

Format (`Code/BF2/Platform/macOS/ProtectedData.h`): `"OBF2"`, version 1, a 16-byte random IV,
AES-256-CBC with PKCS#7 padding of the text including its terminator (as DPAPI is given it),
then HMAC-SHA256 (encrypt-then-MAC, constant-time compare) over all of it. Keys: 64 random bytes
(`arc4random_buf`) in `~/Library/Application Support/OpenBF2/ProtectedData.key`, created on the
first protect with `O_CREAT | O_EXCL | O_NOFOLLOW`, accepted only as a regular 64-byte file. A
missing key file, a foreign blob (DPAPI's start with `01000000d08c9ddf`), a wrong key or any
damage fails the decode. The callers keep the shipped flow, including the unfreed decode buffer
on failure and the null-blob write of an empty unmarked CD key.

Test: `Code/Tests/macos_protected_data` (`[TEMP]`) builds the production `PasswordCodec.cpp`,
`CdKeyStore.cpp` and `ProtectedData.cpp` and runs with `HOME` under `out/` (it refuses any
other home): round trip through the `Profile.con` hex, two encodings differ, key-file mode 0600
and size 64, a flipped nibble, a DPAPI-shaped blob, a missing and a replaced key file, and
`translateCdKey` both ways. All pass.

**Registry source (owner decision, 2026-11-01).** POSIX clients read registry values
from `@HOME@/Registry.con`, on macOS `~/Library/Application Support/OpenBF2/Registry.con`,
with no Keychain dependency. See [file syntax and CD-key setup](macos-write-paths.md#registrycon-syntax).
Missing entries keep the existing absent-key defaults; a missing CD key still reaches
the existing empty-key translation fault on join. The dedicated ELF registry body and
Windows registry code remain unchanged. This replaces the temporary loopback environment
key source; the harness writes a placeholder into its isolated run home's file instead.

## Frontier for milestone 3: a Metal renderer

### What the renderer image is

`RendDX9.dll` is not only the Direct3D device layer. Its plugin entry registers four
modules: `DX9RendModule`, `RendModule`, `geom::ModuleGeom` and
`geom::ModuleGeomMisc`. Geometry (meshes, terrain, particles, decals, overgrowth,
lightmaps, fonts, menu rendering) is therefore part of the renderer image. Its only
exports are `initDll` and `deinitDll`; everything else crosses through
`IClassManager` singletons and class factories.

| Directory | Files | Lines | Lines naming D3D9 types | Lines naming D3DX |
| --- | --- | --- | --- | --- |
| `RendDX9/` | 45 | 27,985 | 439 | 253 |
| `Geom/` | 134 | 85,393 | 295 | 200 |
| `GeomMisc/` | 22 | 8,976 | 30 | 18 |
| `RendMisc/` | 8 | 1,648 | 7 | 19 |
| `Code/RendDX9Abi/` | 32 | 4,127 | ABI shims, `vc71::` containers, D3DX import library |

### Interface surface the engine uses

These are the renderer-side interfaces engine code outside the renderer image names,
with reference counts in `Game`, `Menu`, `Scene`, `IO`, `Core` and `Sound`:

- `dx9rend::IRenderer` (29 references; 52 declared virtual methods in the preserved
  slot order, 27 of them still positional `slot_0x..` names) and `dx9rend::IRenderView` (22; 36 virtual methods), in
  `RendMisc/IRenderer.h` and `RendMisc/IRenderView.h`.
- `IVideoOptions` and the display-mode records in `RendDX9Abi/DisplayModeAbi.h`.
- Geometry interfaces published by the same image: `geom::IMemeTexture` (34),
  `IParticleSystemInstance` (19), `IGeometryTemplate` (17),
  `IParticleSystemManager` (12), `IGeometry` (10), the particle template family,
  `IDecalManager`, `IGeometryTemplateManager`, `IQuadDrawer`, `IPortalMesh`,
  `ILightGeometry` and `IScaleable`.
- The menu and HUD draw path through `ISwiffRender` and `GeomMisc/MenuRend`.

A Metal renderer can keep all of these interfaces and replace the code beneath them.
Inside the image, Direct3D 9 is called directly from many TUs; there is no internal
device abstraction to retarget. The two realistic routes are:

1. **A device seam inside RendDX9:** introduce a narrow internal device interface
   covering the D3D9 calls actually made (resource creation, lock/unlock, render
   targets, state, vertex declarations, draws, queries) and implement it with Metal.
   Most geometry code keeps its structure.
2. **A D3D9-to-Metal translation layer** beneath unchanged renderer code. Less renderer
   churn, but it must emulate D3D9 state semantics wholesale.

Route 1 is the recommended start: the call surface is countable from the source, and it
keeps the shipped renderer logic readable.

### Shaders

The content shaders are 101 D3DX Effect files (`.fx`, plus `.dfx` and `.mfx`) in
`mods/bf2/Shaders_client.zip`, compiled at run time through `ID3DXEffect` from
`d3dx9_25.dll`. Metal cannot consume them. Options: translate HLSL to Metal Shading
Language ahead of time with an external tool chain (none is installed on this machine;
installing one needs the owner's approval), or hand-port the effects, keeping the Effect
parameter names the engine sets. The effect framework itself (techniques, passes, state
assignments, parameter binding) must be reimplemented. Without full Xcode there is no
offline `metal` compiler; Metal libraries can be compiled at run time from MSL source
with `MTLDevice newLibraryWithSource:options:error:`, which is enough for development.

Textures are DDS with DXT1/3/5 compression, which maps to Metal's BC1-BC3 formats on
macOS GPUs (both Intel Macs and Apple silicon). D3DX helpers for texture loading,
mesh utilities and math also need native replacements.

### 32-bit and MSVC assumptions that block a 64-bit Clang client

| Blocker | Extent | Effort |
| --- | --- | --- |
| Client targets exist only in the MSVC x86 configuration (`bf2_client`, plugin `.cmd` scripts, `/FORCE` link) | `CMakeLists.txt` client section, `Code/Build/*.cmd` | 1 week to define a macOS client target and plugin libraries; plugins could become static libraries on macOS, since retail-DLL swapping is already retired (issue #7) |
| MSVC x86 inline assembly | 30 files; 19 HUD files in `Menu/` use `__asm` x87 blocks unguarded, plus renderer, particle, overgrowth and AI files | 1-2 weeks: each needs a C++ arm that reproduces the x87 comparison semantics it documents |
| `vc71::` containers and fixed-layout host views | About 3,400 references in 170 files, mostly `Geom/` (2,362) and `RendDX9/` (785) | 3-5 weeks: replace with native STL across the plugin boundary, as issue #7's staged cleanup plans |
| Retail-size layout assertions (`static_assert(sizeof(...) == 0x..)`) | About 270 in 135 files | 1-2 weeks: give each a native-64-bit branch, as `FileManager.h` does, after measuring |
| Calling-convention spellings and factory thunks (`__stdcall`, `__thiscall`, `__declspec`) | About 1,640 occurrences in 172 files | Mostly mechanical under `PlatformCompat.h`; the thiscall factory thunks in `RendDX9Abi` need real rework |
| `HostDisplayMode` and other x86 ABI records shared with the renderer | `RendDX9Abi/DisplayModeAbi.h`, `HfeAbi.h` | Days, once the plugin boundary is native |
| Proprietary Windows-only runtimes: Bink, Novint HFX, `d3dx9_25.dll` | See above | Disable or replace |

### Rough overall estimate

Milestone 2 (window, input, audio, a client target that links on macOS with a null
renderer): about 6-10 engineer-weeks, dominated by the 64-bit/Clang client blockers
rather than by Cocoa itself. Milestone 3 (Metal device seam, effect framework,
shader translation for 101 effects, D3DX replacements): about 3-6 engineer-months
before a first map renders, with shader translation and effect-state fidelity as the
largest uncertainty. These figures are planning estimates, not measurements.
