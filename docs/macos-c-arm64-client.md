# c-arm64-client: the renderer-ON macOS client for arm64

This package cross-compiles the full macOS client, renderer ON, for arm64 with Apple clang
on the Intel build Mac, and builds a bundle for the owner's M2. All changes are
`[CHOSEN, not recovered]` port code behind platform `#if`s. No MSVC x86 arm changed.
Nothing here has run on Apple silicon. The bundle is how that run happens. The rend track's A1
refresh (below) rebuilt it at the rend-track head and added scripted and hand-play loopback
runs; the earlier sections record the original package.

Base: `mac-metal` 9844a04, branch `mac/c-arm64-client`. Toolchain: Apple clang 17.0.0
(clang-1700.6.3.2), macOS 15.7.7 SDK, Unix Makefiles, RelWithDebInfo, C++14.

## Build steps (original package)

For the current route, see "A1: rebuild at the rend-track head" below.

There is no separate preset. The `macos-x64` preset with an architecture override is the
documented S1a/S2.10 route:

```sh
cmake --preset macos-x64 -B out/build/c-arm64-client-arm64 \
  -DBF2_MACOS_LINK_RENDDX9=ON -DCMAKE_OSX_ARCHITECTURES=arm64 -DBUILD_TESTING=ON
cmake --build out/build/c-arm64-client-arm64 --target bf2_client bf2 bf2_aidll \
  dice_py_check bf2_aidll_loader_check macos_gfx_forward \
  bf2_native_vtable_check -j7
# Metal device harness (standalone project, G3/G3b)
cmake -S Code/Tests/macos_metal_device -B out/build/c-arm64-client-harness-arm64 \
  -G 'Unix Makefiles' -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/c-arm64-client-harness-arm64 -j2
# M2 bundle (the fx cache is the S2 real-effect test cache)
bash Code/Tests/macos_c_arm64/package.sh out/build/c-arm64-client-arm64 \
  out/build/c-arm64-client-harness-arm64 \
  <s2-fx-runtime worktree>/out/m2/s2-fx-runtime/cache
```

Result: every target builds and links strictly. All 12 staged Mach-O files are thin
arm64 and ad-hoc linker-signed (`codesign -v` passes). `bf2_client` is 25.3 MB, `bf2`
14.2 MB, `libaidll.dylib` 1.1 MB and `libdice_py.dylib` 1.8 MB. The client's only
non-system dependency is `@rpath/libdice_py.dylib` (rpath `@loader_path`). The vtable
audit gate checks 118 pairs with the arm64 compile flags: 0 failures.

## Fixes

| Commit | Change | Why |
| --- | --- | --- |
| 05913a7 | `Menu/{BF2HudManager,BF2HudManagerChatInput,BfListBoxData,BfNewListBoxNode,HudQuestionManager}.cpp`, `Geom/ParticleSystem/ParticleSystemManager.cpp`: include `emmintrin.h`/`xmmintrin.h` only under `_WIN32 && !_M_IX86` (it was `!_M_IX86`). | Only their Windows x64 arms use SSE. On arm64 the header is an `#error`. The vtable audit's ParticleSystemManager probe failed first. MSVC x86 and x64 see the same preprocessor result as before. |
| 05913a7 | `Menu/HudVehicleInformation.cpp` (POSIX arm): `bf2x87::abs` instead of `std::fabs` on a `bf2x87::Real`. | `Real` is `ExtendedFloat` on arm64, and `std::fabs` has no overload for it. On Intel `Real` is `long double`, and `abs` is the same `std::fabs`. |
| 05913a7 | `CMakeLists.txt` `bf2_vtable_audit`: when `CMAKE_HOST_SYSTEM_PROCESSOR` is not in `CMAKE_OSX_ARCHITECTURES`, the gate runs the static audit and does not try to execute `bf2_native_vtable_check`. | An Intel host cannot run an arm64 binary. The static half still gates every arm64 build. The native half ships in the M2 bundle. Native and universal trees are unchanged. |
| 4272ca6 | `Platform/X87Compat.h` adds `bf2x87::lowUInt32` (64-bit truncation, low 32 bits). macOS arms use it in `ServerGameLogic.cpp` (`sgl_setParam` "tickets") and gameswf (`>>>` in `gameswf_action.h`, `fromCharCode`, `textColor` ×2, `borderColor`, `backgroundColor`, TextFormat `color`). | A negative `double` cast to `unsigned` wraps on x86 (PE `_ftol2` and x86-64 `cvttsd2si`: -1 gives 0xFFFFFFFF). arm64 `fcvtzu` saturates it to 0. These sites take unclamped script values. Other arms are unchanged. |
| e82fb62 | `X87Compat.h` (Intel arm): `(std::isfinite)(value)`. | gameswf's `base/tu_math.h` defines a function-like `isfinite` macro. The new gameswf include broke four Intel TUs (found by the layout comparison below, fixed and rechecked). |
| 6baa94d | `Code/Tests/macos_c_arm64/{package.sh,run.sh,README.txt}` | The M2 bundle. |

Intel check of the touched TUs: all seven fix-1 TUs, `ServerGameLogic.cpp` and the four
gameswf TUs compile for x86_64 in `out/build/c-arm64-client` (renderer ON). After the
`isfinite` fix, the layout tool's x86_64 pass over all 1,112 TUs has 0 failures. No full
x86_64 relink was done in this package.

## arm64 sizes

`tools/macos/layout_diff_arm64.py` compiles each of the 1,112 unique engine, client,
renderer, Swiff, audio, AI and GameSpy TUs from the compile database twice, for arm64
and for x86_64, with `-fdump-record-layouts-simple`. It compares every record that both
dumps contain: 290,012 record layouts in all, including sizes, alignments, field
offsets and base offsets. Python, zlib and Vorbis are excluded.

- **Project records: 0 differences.** Every BF2/RendDX9/Swiff/AI/GameSpy struct and class
  has the same size, alignment and offsets on arm64 as on x86_64. So the measured native
  (LP64) branches of the 1,240 size/offset assertions hold on arm64 unchanged. No
  assertion needed an arm64 branch, and none fired in the arm64 build.
- libc++ internals only (22 types): `long double` (16 → 8 bytes), the
  `chrono::duration<long double>` family, `__struct_double`, `aligned_storage<24>`
  (`max_align_t` alignment 16 → 8), `std::function` (48 → 32 bytes), and
  `basic_string` field offsets. On arm64 libc++ uses the alternate string layout:
  same 24-byte size, different internal order. No project record embeds one of the
  types that change size. Nothing in the tree reads string internals; PR #55 replaced
  the vc71 containers.

## Static checks of arm64 behavior

| Area | Result |
| --- | --- |
| `char` signedness | Signed on both (`__CHAR_UNSIGNED__` undefined for `-arch arm64` and `-arch x86_64`). `wchar_t` is 4 bytes and signed on both. No difference. |
| `long double` | arm64: 8 bytes, 53-bit mantissa. x86_64: 16 bytes, 64-bit mantissa. All 9 S2.10 files that use `long double` select `bf2x87::Real` under `__APPLE__`. The remaining `long double` text is in non-Apple arms or X87Compat. There are no `strtold`, `*l` libm calls or `%Lf`, and no project struct contains a `long double` (layout check). |
| `__BIGGEST_ALIGNMENT__` | 16 on x86_64, 8 on arm64. It affects only libc++ `max_align_t` users (above). malloc is 16-byte aligned on both. |
| Float-to-unsigned conversion | New AST scan (`tools/macos/float_to_unsigned_scan.py`): 49 explicit or implicit FloatingToIntegral casts to 32/64-bit unsigned types in project code. 33 are clamped quantizers (25 network/player codecs) or Metal/effect conversions of non-negative or clamped values (8). For those, NaN also gives 0 on both architectures, so nothing differs. The 8 script-reachable unclamped sites are fixed (4272ca6). Left unchanged (8): `DestroyableObjectNetworkable.cpp:168` (hit points plus max wreck hit points; non-negative unless the armor invariant breaks), `SupplyDepot.cpp:241` (ammo ≥ 0; -1 is handled before), the settings-driven delays in `ServerGameLogic.cpp:459/516/750` and `GameClient.cpp:1723`, `EndOfRoundInfo.cpp:244`, and `CocoaHost.mm:27`. |
| Float-to-signed out of range | Not scanned site by site. x86_64 gives INT_MIN, arm64 saturates, NaN gives 0. The S2.10 41-file inventory already routes x87 conversions through `bf2x87::integer`. The remaining plain `int` casts of in-range values are equal. The scanner can be pointed at signed types if a site shows up. (Retail MSVC `_ftol2` wraps through 64 bits, so x86_64 already differs from retail there. That is not arm64-specific.) |
| Unaligned access | BitStream is byte-wise with exact-buffer reads (S2.10 threads). The zip reader is minizip `unzip.c`, which reads byte by byte through `unzlocal_getByte`. `SWConnection` uses `memcpy`. The mesh and terrain builders (`StaticMeshMaterial`, `SkinnedMesh`, `QuadPatch`, `BundledMeshTemplate`, `LightmapGeneration`) cast into their own vertex and stat buffers. ARMv8 permits unaligned ordinary loads and stores to Normal memory, and Metal shared or write-combined buffers are Normal memory. Only exclusive/atomic accesses fault, and S2.10 showed that no atomic target aliases packed or file data. No fix needed. |
| Memory ordering | Every engine thread was checked. `Core/Thread` is published and joined through `pthread_create`/`pthread_join`. NetServer, AutoRecorder, the Music/Win32System worker, GameSpy and Python are S2.10 atomics. `VoipServerInternalHookThread::m_stopFlag` is written and read under its shared mutex. `CheckServerAliveThread::run` is an empty frontier. The Metal bridge has no completion handlers: it waits synchronously with `waitUntilCompleted`. Cocoa/GameController callbacks run on the main thread, which also pumps them. OpenAL is called from the engine threads only. There are no lock-free queues, `volatile` flag handoffs or barrier intrinsics outside the S2.10 inventory. No new issue. |
| Calling convention / ABI | NativeVtable adapters and `sr_getRandomCreditsName` call free functions with the compiler-supplied sret. AAPCS64 (x8) and SysV both treat member and free functions the same way, so the static-slot dispatch is valid on arm64. The runtime proof is `bf2_native_vtable_check` on the M2. There are no casts to variadic function-pointer types. `va_list` (`char*` on arm64) is never reused without `va_copy` in engine code, and Python already has its `VA_LIST_IS_ARRAY` override. |
| CPU probes / timers | `__rdtsc`/CPUID sit only in `_WIN32` arms. `System.cpp` and `RagDollTemplate.cpp` read `cntvct_el0` on `__aarch64__`. `VideoOptions` uses sysctl (S2.10). Note: `hw.cpufrequency` does not exist on Apple silicon, so `cpuBenchmarkSpeed` would stop at the `macos-arm-quality-calibration` frontier if quality detection is reached. |
| Page size | 16 KB on arm64. No hard-coded 4096 page size with mmap/mprotect. dlmalloc uses `sysconf`. |
| Metal on Apple GPUs | The depth format falls back to `Depth32Float_Stencil8` when `depth24Stencil8PixelFormatSupported` is false (always false on Apple GPUs). BC is checked through `supportsBCTextureCompression`. Shared/private storage only, no managed resources. |
| Integer division by zero | x86 traps (SIGFPE), arm64 returns 0. A latent divide-by-zero would not crash on the M2. Not fixable statically. Listed so that an M2-only "works" is not over-read. |

Seen on the way, not arm64-specific: `NetClient::startReceiveThread` creates its thread
only on `_WIN32`, so a threaded POSIX client connection logs "clientReceiveThread was not
created successfully". Python 2.3.4's `socketmodule.c:3027` `inet_ntop` has a one-byte
`memset` overflow (`sizeof(ip) + 1`, warned by clang). Both are left to their owners.

## A1: rebuild at the rend-track head

Rend track milestone A1, branch `rend/arm64` at rend-track
3ad79c51, built on the Intel Mac (macOS 15.7.7, Apple clang 17.0.0, CMake 4.4.2). There is still
no `macos-arm64` preset. The route is the `macos-x64` preset with an architecture override:

```sh
sh tools/macos/nativefx/build.sh arm64                 # -> out/build/nativefx-arm64
cmake --preset macos-x64 -B out/build/rend-arm64 -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DBF2_MACOS_LINK_RENDDX9=ON -DBF2_FX_NATIVE_DEPS=$PWD/out/build/nativefx-arm64 \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DBUILD_TESTING=ON
cmake --build out/build/rend-arm64 --target bf2_client bf2 bf2_aidll bf2_vtable_audit \
  dice_py_check bf2_aidll_loader_check macos_gfx_forward \
  bf2_native_vtable_check bf2_meme_dispatch_check -j3
F='-UBF2_MENU_FIXTURES -DBF2_MENU_FIXTURES=\"fixtures\"'
cmake -S Code/Tests/macos_metal_device -B out/build/rend-arm64-harness -G 'Unix Makefiles' \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_BUILD_TYPE=Debug -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DBF2_FX_NATIVE_DEPS=$PWD/out/build/nativefx-arm64 "-DCMAKE_CXX_FLAGS=$F" "-DCMAKE_OBJCXX_FLAGS=$F"
cmake --build out/build/rend-arm64-harness -j3
BF2_INTEL_REFERENCE_RUN=<an Intel gate loopback run dir> \
  bash Code/Tests/macos_c_arm64/package.sh out/build/rend-arm64 out/build/rend-arm64-harness
```

Notes on the commands:

- `BUILD_TESTING=ON` is needed only for the check executables in the bundle.
- The Metal harness compiles its fixture directory in as an absolute path into this checkout
  (`BF2_MENU_FIXTURES`), and its default GPU run reads the menu JPEG fixtures from there. The
  flags above replace that path with the relative `fixtures`, and `package.sh` ships the
  fixtures in `bin/fixtures`. `package.sh` refuses a harness that still names the checkout path.
- `macos_fx_runtime` is no longer in the bundle. At this head it exists only with
  `BF2_FX_DEVELOPER_ORACLE=ON`, and the S2 cache it read belonged to the retired Wine route.
  The production native compiler is exercised by the Metal harness and by every client run.

Results (all compile and link only):

| Target | Result | Size (bytes) |
| --- | --- | ---: |
| `nativefx-arm64` (vkd3d-shader, vkd3d-common, MojoShader, harness) | built, all thin arm64 (`lipo -info`), 1 min 42 s | |
| `bf2_client` (renderer ON) | built | 27,630,952 |
| `bf2` | built | 14,344,608 |
| `bf2_aidll` (`libaidll.dylib`) | built. The arm64 `BotMain.cpp` diagnostic patch is in this head, so no AI-code failure | 3,943,504 |
| `libdice_py.dylib` | built | 1,794,336 |
| `bf2_vtable_audit` | **0 failures**: 135 plugin pairs, AI behaviour checker 256 classes with 0 mismatches | |
| `dice_py_check`, `bf2_aidll_loader_check`, `bf2_native_vtable_check`, `bf2_meme_dispatch_check`, `macos_gfx_forward`, `macos_metal_device` | built, arm64 | |

What the arm64 audit covers on an Intel host:

- `tools/macos/vtable_audit.py` compiles every static-plugin probe with the arm64 target's own
  flags and compares the clang-computed slot maps of the 135 interface/implementation pairs.
- `ai_behaviour_vtables.py` compares the AI behaviour classes against the `libaidll.so` ELF
  manifest.

Both are static. They prove the arm64 compiler lays out the same slots, but they do not prove
that the calls dispatch at run time. `bf2_native_vtable_check` and `bf2_meme_dispatch_check`
cannot execute here: the gate prints that they "run in the M2 bundle", and they are the first
step of `run.sh checks`.

Link warnings are the known ones from r3: the static-plugin prelinked objects are tagged
macOS 15.0 while the link is for 11.0, and `libbf2_fx_runtime.a` appears twice. Every image has
`minos 11.0` (SDK 26.2). Compiler warnings are the pre-existing classes
(`-Wundefined-bool-conversion`, `-Wdeprecated-register` and others). None of them is new in an
arm64 file.

A Homebrew upgrade started by another session removed `/usr/local/bin/cmake` in the middle of the
build. The build was regenerated with a private copy of the same CMake 4.4.2
(`out/tools/cmake-4.4.2`) and finished. The result does not depend on that copy.

## M2 bundle

`package.sh` writes `out/arm64-bundle/openbf2-arm64/` and the archive
`out/arm64-bundle/openbf2-arm64-<hash>.tgz` next to it. Both are ignored by Git and contain no game
content. The bundle root stands in for the repository root, so the loopback harness runs
unchanged from inside it:

| Path | Contents |
| --- | --- |
| `out/build/arm64/bin_macos/` | `bf2_client`, `bf2`, `libaidll.dylib`, `libdice_py.dylib` (the layout `loopback.py --build arm64` expects) |
| `tools/macos/loopback/` | `loopback.py`, `client.sb`, `server.sb`, `server.tcl`, `scripts/*.txt`, copied unchanged except for one line of `loopback.py` (below) |
| `bin/` | `dice_py_check`, `bf2_aidll_loader_check`, `bf2_native_vtable_check`, `bf2_meme_dispatch_check`, `macos_gfx_forward`, `macos_metal_device` and its `fixtures/`, plus copies of both dylibs |
| `check-lib/` | `copy_reg.py` and `types.py` for `dice_py_check` |
| `reference/` | the Intel gate run's frontier set, client result and input-script line count; `loopback.py.diff`; the list of images that contain build-host path strings (debug info and `__FILE__`) |
| `run.sh`, `README.txt`, `artifacts.txt`, `MANIFEST.sha256` | the runner, short instructions, per-image architecture, SHA-256, non-system dependencies and rpaths, and a hash of every file |

The one change to the harness copy: `loopback.py` takes its machine-wide run lock from
`~/Library/Caches/OpenBF2-loopback/` (or `BF2_LOOPBACK_LOCK`). The copy
takes it from `out/run/loopback.lock` inside the bundle, or from `BF2_LOOPBACK_LOCK` if that is set.
`package.sh` refuses to package if the original line has changed. Ports (16890, 29911, with 16567
denied), sandbox profiles, settings and scripts are unchanged.

`package.sh` gates the bundle. It fails if:

- a Mach-O file is not arm64;
- a dependency or rpath points anywhere other than `@rpath`, `@loader_path`, `@executable_path`,
  `/usr/lib` or `/System/Library`;
- a file has no valid signature after `codesign --force --sign -`.

`run.sh [--content DIR] MODE [MINUTES] [-- loopback options]` has these modes:

| Mode | What it does |
| --- | --- |
| `preflight` | Runs first in every mode. Refuses a non-arm64 host and a shell running under Rosetta. Clears `com.apple.quarantine` inside the bundle, checks `MANIFEST.sha256`, then checks every image with `file` and `codesign -v`, re-signing ad hoc if the check fails. Finds Python 3.7 or later (`BF2_PYTHON`, then the bundle's own `python/`, then Command Line Tools `/usr/bin/python3`, Homebrew or python.org), `expect`, `sandbox-exec` and `lsof`. Checks the content folder and warns when it is on a different volume, because the clones then become full copies. |
| `checks` | The checks of the old bundle without `macos_fx_runtime`, plus `bf2_meme_dispatch_check`, each with a time limit and an expected exit code. The three Metal frontier modes must exit 134. |
| `gate` | `infantry-range`, `render-view-dalian`, `render-view-ocs` (`--scenario`), then the map2 reload run, as `tools/tracks/rend_gate.sh` does. Writes per run the frontier count, crash reports, script-finished flag and the view table against `reference/baselines/`, with compare PNGs for every view. Over the Intel limit is information only. |
| `smoke` | `loopback.py --build arm64 --ai --bots 16 --ai-spawn-only --input-script play-combat --client-seconds 200`, the track gate command, on Dalian co-op. The verdict is PASS when the client is still running at 200 s, the server's exit is clean, there is no crash report and no `[FRONTIER]` appears that the Intel gate run did not reach. |
| `smoke-ai` | The same with the full bot AI ticking. This tests the arm64 AI image under load. Bots move, so the script's aim can miss. |
| `play [MIN]` | `--ai --bots 16`, no input script, `--client-seconds MIN*60+300`. Takes present PNGs about once a minute (at most 20) and logs `BF2_PLAY_TRACE` `[PLAY]` lines once a second. |
| `collect` | Packs `results.txt`, `results/` and every run under `out/loopback/run-*` into `results.tgz`. Each run contributes its top-level logs and results, crash reports, `present/` and `client-home/`, plus any file under 20 MB that the game wrote inside its content clones after that run started. Every mode ends with `collect`. |
| `all` | `checks`, `smoke`, `smoke-ai`. |

The summary for each run (`results/<mode>.summary.txt`, also appended to `results.txt`) contains:

- the client and server results;
- the input-script line count against the Intel run;
- the frontiers reached, with the ones the Intel gate run did not reach marked `NEW`;
- the triggered thread of each crash report;
- watch-list lines from the client and server logs (`ExtendedFloat`, `bf2x87`, `SIGFPE`,
  `pure virtual`, `vtable`, `EXC_BAD`/`EXC_ARITH`).

If the harness is interrupted (Ctrl-C), `run.sh` stops the server the same way the harness
does, and still writes the summary and `results.tgz`.

`package.sh` also stages `tools/macos/loopback/{scenarios,pose,compare_views.py}` and the Intel
baselines (`BF2_BASELINES`, default `<repo>/out/baselines`, into `reference/baselines/`). For an
Intel rehearsal, `BF2_BUNDLE_ARCH=x86_64 BF2_BUNDLE_SKIP_CHECKS=1 package.sh X86_64_BUILD - DEST`
stages an x86_64 build under `out/build/x86_64` (recorded in `build-tag.txt`, which `run.sh`
passes to `--build`) without the check tools.

`BF2_BUNDLE_DRYRUN=1` prints the harness commands without starting them.
`BF2_BUNDLE_REHEARSAL=1` (`[TEMP]`, Intel rehearsal only) reports a wrong architecture or a
manifest mismatch without stopping on it.

What was verified on the Intel Mac (no arm64 code ran):

- **Bundle and archive.** `out/arm64-bundle/openbf2-arm64-3ad79c51.tgz` is 17 MB (59 MB
  unpacked). A fresh extraction passes `shasum -c MANIFEST.sha256`.
- **Architecture.** `lipo -archs` reports `arm64` for all 12 Mach-O files.
- **Signatures.** `codesign -v` passes for every file, and every signature is ad hoc.
- **Dependencies.** `otool -L` lists only `@rpath/libdice_py.dylib` and `@rpath/libaidll.dylib`
  (the dylib IDs) plus `/usr/lib` and `/System/Library`, with rpath `@loader_path`. No path into
  a build tree or Homebrew appears. The 12 images contain this Mac's source paths only as debug
  information and `__FILE__` strings (`reference/build-path-strings.txt`). After the fixtures
  rebuild, no runtime file path remains.
- **Minimum OS.** All images are `minos 11.0`.
- **Script syntax.** `bash -n` passes for `run.sh` and `package.sh`, and `run.sh` runs under
  `/bin/bash` 3.2.
- **Preflight.** It cleared two files that had `com.apple.quarantine` set on purpose. It
  re-signed unsigned images and reported them, and it refused a missing content folder,
  listing the paths it tried.
- **Dry run.** `BF2_BUNDLE_DRYRUN=1` prints the exact harness commands for `smoke`, `smoke-ai`
  and `play`, including extra options passed after `--`.
- **`checks` on Intel.** With the arm64 binaries, every step launched, was recorded as FAIL
  ("Bad CPU type", as expected) and went into `results.tgz`.
- **Full rehearsal.** In a throwaway copy, the four game images were replaced with this head's
  x86_64 build (`rend-track`), and the shared machine lock was used through
  `BF2_LOOPBACK_LOCK`. `BF2_BUNDLE_REHEARSAL=1 ./run.sh smoke -- --client-seconds 90` ran the
  real harness from inside the bundle. The server reached playing in 7.9 s. The client joined,
  spawned and ran the script to its 90 s limit: 19 report lines, 17 PNGs and 16 bots
  (`players t1 8/8 t2 9/9`). The server exited cleanly (`EXIT ... 0 0`), and no crash report
  was written.
  - The run reached the same three frontiers as the Intel gate run (SwiffRender fonts ×2 and
    `ModuleGeomMisc::init`), and none that the Intel run did not. The verdict was PASS.
  - `results.tgz` was 12 MB. It held the logs, PNGs and profiles, and no game-derived shader
    cache or `.pyc` files.

## Owner instructions (M2, milestone A2)

Before you start:

- The M2 needs a logged-in desktop session, Terminal, and about 2 GB of free space on the volume
  that holds the game. Each run clones the game folder; clones take little space.
- The harness needs Python 3.7 or later. A bundle staged with `BF2_BUNDLE_PYTHON` (package.sh)
  carries a portable CPython (python-build-standalone) in `python/` and needs neither Xcode nor
  the Command Line Tools; macOS's own `/usr/bin/python3` is only a stub that offers that download.
  Without `python/`, install the Command Line Tools (`xcode-select --install`) or point
  `BF2_PYTHON` at another `python3`. The checks alone do not need Python.
- Nothing else on the M2 may use UDP ports 16890 or 29911 during the runs. The harness waits up
  to 10 minutes for them and then refuses to start. Port 16567 is never used.
- If macOS asks whether `bf2` may accept incoming network connections, either answer works. The
  runs use loopback only.

Steps:

1. Copy `openbf2-arm64-<hash>.tgz` to the M2 (AirDrop, USB stick or `scp`).
2. Unpack it on the same volume as the game, outside the game folder and outside
   Desktop/Documents/Downloads:
   ```sh
   mkdir -p ~/OpenBF2-m2 && tar -xzf ~/Downloads/openbf2-arm64-*.tgz -C ~/OpenBF2-m2
   xattr -dr com.apple.quarantine ~/OpenBF2-m2/openbf2-arm64
   cd ~/OpenBF2-m2/openbf2-arm64
   ```
   Short form for the renderer gate (the same standard tests as `tools/tracks/rend_gate.sh` on
   the Intel gate machine): unpack (a bundle without `python/` also needs the Command Line Tools, see above), then
   ```sh
   /usr/bin/arch -arm64 /bin/bash ./run.sh --content "<Battlefield 2 folder>" gate
   ```
   and AirDrop `results.tgz` (in the bundle folder) back. Do not touch the mouse or keyboard
   while it runs (about 15 minutes; four game windows open one after another).
   `gate` runs `infantry-range`, `render-view-dalian` and `render-view-ocs` through
   `loopback.py --scenario`, then the map2 reload run (Dalian, `admin.runNextLevel` at 120 s to
   operation_clean_sweep, 300 s). Each run's entry in `results.txt` gives the `[FRONTIER]` line
   count, crash reports, whether the scenario script finished, and, for the two render-view
   scenarios, a per-view table (`compare_views.py --all-pngs`) against the Intel reference views
   in `reference/baselines/<scenario>/`. The reference views are the accepted Intel captures from
   `out/baselines` (`package.sh` takes another folder from `BF2_BASELINES`). The M2 has a
   different GPU, so a view over its Intel limit is reported as information and does not fail the
   gate; the verdict is `CHECK` only for a frontier, a crash report or an unfinished script. A
   side-by-side and heatmap PNG is written for every view to `results/compare/<scenario>/`.
   `results.tgz` holds `results.txt`, those PNGs, every run's view captures (`present/`), client
   and server logs and crash reports, and no game content.
3. Preflight (a few seconds):
   ```sh
   /usr/bin/arch -arm64 /bin/bash ./run.sh preflight
   ```
   It must print `preflight OK` and the content folder it found. If the game is not in
   `~/Applications/Battlefield 2.app/Contents/drive_c/Battlefield 2`, add
   `--content '/path/to/Battlefield 2'` to this and every later command. That folder holds
   `mods/`, `python/` and `pylib-2.3.4.zip`. A refusal names exactly what is missing.
4. Checks and both scripted runs (about 15 minutes; the first run compiles all effects):
   ```sh
   /usr/bin/arch -arm64 /bin/bash ./run.sh all
   ```
   While a scripted run is going, do not touch the mouse or keyboard. A game window opens at
   800x600 and plays by itself: it joins, spawns, aims and fires, dies and respawns. Each
   step prints `== PASS` or `== FAIL`, and each run prints its summary.
   - If `native_vtable_check` or `meme_dispatch_check` fails, that is the vtable finding. Stop
     there and send `results.tgz`.
5. Hand play, about 10 minutes, using the checklist below:
   ```sh
   /usr/bin/arch -arm64 /bin/bash ./run.sh play 10
   ```
   The window opens once the server has loaded Dalian. Note the wall-clock time of anything odd.
   To end, use Esc > Quit, or let the time run out. Do not close Terminal while it runs.
6. Send back `~/OpenBF2-m2/openbf2-arm64/results.tgz`, together with your notes. Every mode
   rewrites it with everything collected so far.

Hand-play checklist (with `play`):

1. Window to spawn screen: how long did it take, and did the loading screen and menu look right?
2. Spawn as each of two kits. Walk, sprint, crouch, go prone, jump, and look around with the mouse.
   Does movement feel smooth, and is there any input lag or a stuck key?
3. Fire, reload and switch weapons. Throw a grenade and watch the explosion (particles, physics
   and sound).
4. Kill a bot and get killed. Respawn at a different spawn point.
5. Take a vehicle near your base (jeep, APC or tank): drive, use the turret, get out. Fly a
   helicopter if one is near.
6. Open the scoreboard (Tab), the commo rose (Q) and the in-game menu (Esc), and send one chat
   line.
7. Check the HUD numbers: ammo, health, tickets, the minimap and the kill messages. Note any
   value that is 0, negative or plainly wrong.
8. Look into the distance and at the ground: terrain, shadows, water and textures. Name anything
   that differs from the Intel Mac.
   - Too dark near the ground is a known gap (B3); note it but do not report it as new.
9. Sound: weapons, vehicles, voices and 3D direction.
10. Frame rate in heavy scenes. Stutter in the first minutes is the effect compiler and is
    expected; stutter after that is not.
11. Quit through Esc > Quit. Did the game close cleanly?

What to watch for on arm64:

- **x87 and `ExtendedFloat`.** On Intel, `bf2x87::Real` is the hardware `long double` (64-bit
  mantissa). On arm64 it is the software `ExtendedFloat`. The signs of trouble are:
  - a crash stack with `bf2x87` or `ExtendedFloat` frames;
  - physics that differs from Intel (projectile drop, vehicle handling, rag dolls);
  - timers or HUD numbers that drift or show wrong values.
- **The vtable check.** `bf2_native_vtable_check` and `bf2_meme_dispatch_check` run for the
  first time on arm64 (the Intel host runs only the static audit). A failure there, or a crash
  in a virtual call (`EXC_BAD_ACCESS` near a small address, `pure virtual method called`) in a
  menu or the HUD, is an ABI finding.
- **Division by zero.** An integer division by zero traps on x86 (SIGFPE, `EXC_ARITH`) and
  returns 0 on arm64. So a session that works on the M2 where the Intel Mac crashes with
  `EXC_ARITH` is a finding, not a pass. So are zeros that appear on the M2 only (percentages,
  bars, tickets).
- **Float-to-unsigned conversion.** arm64 saturates negative values to 0, where x86 wraps them.
  The eight script-reachable sites use `bf2x87::lowUInt32`. Colours or counters that read 0 on
  the M2 only point at a site the scan missed (table above).
- Anything marked `NEW` in a summary, any crash report, and any `FAIL` in the checks.

## Questions for the owner

- Where does the Battlefield 2 1.50 content live on the M2? The default assumes the same Wine
  wrapper as the Intel Mac: `~/Applications/Battlefield 2.app/Contents/drive_c/Battlefield 2`.
  It must sit on the same volume as the bundle so that it can be cloned.
- How should files move between the Macs: AirDrop, a shared folder, `scp` or a USB stick? The
  bundle is 17 MB (59 MB unpacked) and `results.tgz` is about 10 to 30 MB, mostly screenshots.
- The harness needs Python 3.7 or later; current bundles bring their own (`python/`), so the
  Command Line Tools are not needed.

## Open items

- No M2 execution yet. Every arm64 claim above is compile, link, or static. The A1 bundle is
  ready for the owner's A2 run (instructions above).
- The `run.sh` watch list is a text filter. It finds named symptoms in the logs but does not
  replace reading the summaries and crash stacks.
- No full x86_64 relink of this branch (touched TUs compiled for x86_64 only). The
  coordinator's merge gate should rebuild `bf2_client` and `bf2` for x86_64.
- Signed float-to-int out-of-range sites are not inventoried (see the table above).
- MSVC x86/x64 and Linux are not checked here. All edits are inside `__APPLE__`, in macOS
  or cross-build CMake branches, or in includes guarded by `_WIN32 && !_M_IX86` (the same
  result on MSVC).
