# macOS client plan

The plan for bringing the OpenBF2 client to macOS, ending in a native Metal renderer. Work happens on the branch `mac-metal` of `kr4uzi/OpenBF2`, and every port change is labelled `[CHOSEN, not recovered]` (see AGENTS.md). `docs/macos-port.md` holds the measured analysis this plan is based on: sizes, file lists and the interface surface.

## Target platforms

The client ships as one **universal binary** (`arm64` + `x86_64`). **Apple silicon is the primary target**: it is where the current route (retail client under Wine and Rosetta 2) is slowest, and where a native build gains most.

Measured on a MacBook Air M2 (8 GB), retail 1.50 under Wine 10 with Rosetta 2, single player, Strike at Karkand (see "Performance target"):

- about 9 FPS with the default audio settings, about 17 FPS with hardware audio and EAX off; the Intel Mac runs the retail client at the 60 FPS vsync cap (reported by the owner; not measured with this benchmark).
- The busy threads (game thread, OpenAL mixer) spend most of their time in Rosetta's runtime, not in translated code. A controlled test with the same Wine and Rosetta: 400 M iterations of `fadd`/`fmul` take 14.9 s with x87 and 0.9 s with SSE, and the x87 test hits the same Rosetta runtime addresses as BF2. Rosetta translates SSE to native code but runs x87 through runtime helpers. The retail engine is x87 code throughout (MSVC 7.1, and the client runs the x87 unit in single precision, see `Geom/TerrainDraw.cpp`).
- The GPU side is mostly idle. A native `arm64` engine removes the bottleneck; the renderer is needed to have a client at all, not for speed.

Two machines are available: an Intel MacBook Pro (i7-8569U, Iris Plus 655, x86_64, has the toolchain) and a MacBook Air M2 (8 GB, arm64, no Command Line Tools). Until the owner decides to install the CLT on the M2, `arm64` binaries are cross-compiled on the Intel Mac and **run** on the M2. An `arm64` result counts only when it ran on the M2; "compiles and links" is recorded as such.

## Status (2026-10-22)

| Package | Status |
|---|---|
| S2.0 client target | done (merged). Universal preset (item 4) still open: goes with 1a |
| S2.1 inline assembly | Codex, running; must adopt the S2.10 helpers afterwards |
| S2.2 size assertions | done (merged, x86_64 measured; arm64 sizes not yet measured). Left: `SwiffPlayer.cpp` (2 checks) and `BF2Render.cpp` (1 check) |
| S2.3 calling conventions | no compile errors of this class (S2.0 triage); folded into S2.7 and stage 3 |
| S2.4 Cocoa host and input | done (merged); open: raw mouse, Retina/internal resolution per "Display", IME, Spanish ISO test, M2 run |
| S2.5 audio | done (merged, Intel only) |
| S2.6 64-bit AI library | builds and loads (merged); co-op bot smoke not run yet |
| S2.7 vc71 / plugin globals | first part done (merged): native-width vc71, prelinked static plugins; renderer trees remain |
| S2.8 disabled runtimes | done (merged) |
| S2.9 first client link | Claude, paused at the weekly budget; client compile is down to 2 TUs |
| S2.10 arm64 numerics/threading | not started |

Running anything on the M2: agents can't reach it. A package that needs an M2 run produces a self-contained test bundle under `out/m2/<pkg>/` (binaries cross-compiled for arm64, a `run.sh` that writes `results.txt`); the owner runs it on the M2 and drops `results.txt` back into the same folder.

## Stages

| Stage | Result | Status | Estimate |
|---|---|---|---|
| 1. Headless engine | `bf2` dedicated server builds, boots, loads a map and quits cleanly on macOS (x86_64; arm64 links) | done (commits 7cb05bc..f1d7849) | — |
| 1a. arm64 run | `dice_py_check_run` and the server smoke (same sandbox profile, port 16890, offline) **run on the M2**; `arm64` results recorded in `docs/macos-port.md` | planned | days |
| 2. Client build and platform layer | the client builds as a universal 64-bit binary with Apple clang, opens a Cocoa window, takes keyboard/mouse input and plays OpenAL audio, with a null renderer; runs on both Macs | in progress | 6–10 engineer-weeks, plus 1–2 weeks for S2.10 |
| 3. Metal renderer | a Metal device seam inside the renderer image, the effect framework, 101 translated shaders, and replacements for the D3DX helpers. Goal: a map renders on both Macs | planned | 3–6 engineer-months |
| 4. Playable client | menus (SwiffPlayer through Metal), HUD, joining a server, gameplay, and bots via a 64-bit AI library; meets the performance target on the M2 | planned | open |

Stage 1a goes first: it is cheap, and it is the first time any OpenBF2 code runs on Apple silicon. Problems found there (numerics, threading, alignment) are cheaper to fix before the client packages build on top.

## Stage 2 work packages

Each package gets its own git worktree, `<pkg>`, on the branch `mac/<pkg>` off `mac-metal`. The coordinator merges finished packages into `mac-metal` after review. **For every package, "done" includes: builds for `arm64` and `x86_64`, and its tests ran on both Macs.**

| Package | Content | Depends on | Who | Done when |
|---|---|---|---|---|
| S2.0 client target | 1. A `bf2_client` CMake target for macOS (DEDICATED=0) that links the engine with a null renderer, null input and null sound. 2. The renderer, sound and movie plugins become static libraries on macOS, because retail-DLL swapping is already retired (issue #7). 3. Compile triage: an error list per directory and per blocker class. 4. `CMAKE_OSX_ARCHITECTURES="arm64;x86_64"` in the macOS client preset. | stage 1 | Claude (Opus) | the target configures, compiles everything that is not blocked for both architectures, and the blocked rest is listed by blocker class in `docs/macos-port.md` |
| S2.1 inline assembly | C++ versions of the MSVC x86 `__asm` blocks (30 files; 19 x87 HUD blocks in `Menu/`), keeping their documented x87 comparison semantics. The x86 MSVC code stays as it is behind `#if`. The float→int and comparison helpers come from S2.10, so every block uses the same definitions. | S2.10 helpers | Codex | no unguarded `__asm` is left in client sources; each new path has a small unit check under `Code/Tests` that **ran on the M2** |
| S2.2 retail-size assertions | Native 64-bit branches for the about 270 `sizeof == 0x..` checks, measured with the S2.0 target, following the pattern in `FileManager.h`. Sizes are measured for both architectures (they normally agree; any difference is recorded). | S2.0 | Codex | the client TUs compile past their size checks on macOS for both architectures |
| S2.3 calling conventions | `__stdcall`/`__thiscall`/`__declspec` spellings handled via `PlatformCompat.h`. The RendDX9Abi thiscall factory thunks are listed for rework. | S2.0 | Codex | no compile errors of this class are left |
| S2.4 Cocoa host and input | `NSApplication`/`NSWindow` host with a `CAMetalLayer` (empty for now), focus/fullscreen handling, and the message-pump replacement for `WinMain`/`BF2Window`. Keyboard and mouse events (`NSEvent`, raw mouse deltas) are mapped onto the existing `DIK_` codes, so control maps and `.con` bindings stay valid; the mapping uses key codes (physical keys), so non-US layouts work, including the console key. Game controllers via `GCController`. **Display: no mode switching** (see "Display" below). | — (develop against a small test app; wire it in once S2.0 links) | Claude (Opus) | a test app shows the window and logs DIK codes on both Macs, including a Spanish ISO keyboard; once S2.0 links, the client opens its window |
| S2.5 audio | The BF2Audio OpenAL provider on macOS (`OpenAL.framework`, deprecated but present), with `Win32System.cpp` replaced by a POSIX/macOS arm | S2.0 | Codex | the client plays a menu sound on both Macs |
| S2.6 64-bit AI library | `bf2_aidll` builds as `libaidll.dylib`/`.so` for the 64-bit POSIX ports; the engine loads it. The AIDLL's `long double` code (`IPIMobileKnown.cpp`, `IPIMobileReal.cpp`, `Squad*.cpp`) and its three `__asm` files follow S2.10. | stage 1, S2.10 | Codex | a macOS co-op server boot smoke spawns bots (sandboxed, port 16890, offline) **on the M2** |
| S2.7 `vc71::` containers | Native STL in place of `vc71::` containers and fixed-layout host views across the plugin boundary (about 3,400 references, mostly in `Geom/` and `RendDX9/`) | S2.0 | Claude + Codex, split by directory | starts after S2.0's triage; 3–5 weeks |
| S2.8 disabled runtimes | Bink movies, Novint HFX and VoIP capture disabled on macOS, with visible `[FRONTIER]` logs and no fake success | S2.0 | Codex | the client runs past these points |
| S2.9 first client link | Recover or frontier the 12 symbols that have no definition on any platform (evidence: retail BF2.exe), clear the last client compile failures, first strict `bf2_client` link and a sandboxed startup to the null-renderer exit | S2.0–S2.8 | Claude (Opus) | `bf2_client` links strictly and its startup reach is recorded |
| S2.10 arm64 numerics and threading (Apple silicon review) | See "Apple silicon rules" below: `long double`, the x87 single-precision mode, float→int conversion, memory ordering, `wchar_t` in files, the SSE/`cpuid` sites. Shared helpers in `Platform/` with unit tests, then the call sites. | S2.0 triage | Claude (Opus) | helpers and tests exist and ran on both Macs; every listed site uses them or is recorded as unaffected; a ThreadSanitizer run of the server smoke on the M2 is clean or its findings are listed |

Order: 1a first. Then S2.0, S2.1, S2.4, S2.6 and S2.10 start at once (S2.1 and S2.6 take S2.10's helpers as soon as they exist). S2.2, S2.3, S2.5 and S2.8 follow S2.0's triage, and S2.7 is the long tail.

## Apple silicon rules (S2.10)

These are the places where an `x86_64` build on the Intel Mac and an `arm64` build behave differently. The Intel Mac can't catch them, and the retail client under Rosetta can't either: Rosetta emulates x87 and runs x86 code with x86 memory ordering.

1. **`long double` is 80-bit x87 on `x86_64` and 64-bit on `arm64`.** The tree uses it on purpose to reproduce x87 intermediate precision: 98 code lines in `IO/Network/NetworkStats.cpp`, `Math/Frustum.cpp`, `Scene/Object/PlayerManager.cpp`, `Menu/BF2HudManagerHudEvents.cpp` and the AIDLL (`IPIMobileKnown.cpp`, `IPIMobileReal.cpp`, `SquadCoordinator.cpp`, `SquadFormation.cpp`, `SquadMember.cpp`). On `arm64` these silently become `double`, so the two Mac builds compute different results. Decide per site: plain `double` (and document the precision change), or a small helper that reproduces the retail result where it is observable (AI decisions, network-visible values).
2. **The retail client runs the x87 unit in single precision** (device created without `D3DCREATE_FPU_PRESERVE`, see `Geom/TerrainDraw.cpp` and `Build/textureatlas_diff/host.cpp`). Native `float`/`double` code on either architecture doesn't. Fidelity tests that compare client results with retail use tolerances, and the places that depend on the reduced precision get named.
3. **Float→int conversion.** 41 files use `fistp`/`fldcw` truncation tricks or `_ftol`. For values in range, a C++ cast matches. Out of range and for NaN they differ: x86 returns `0x80000000`, `arm64` saturates (NaN gives 0). One set of helpers (`toIntTruncate`, `toIntRound`) reproduces the x86 result, with unit tests for range edges, ±Inf and NaN on both architectures.
4. **x87 comparisons** (`fcom`/`fnstsw`/`sahf`): NaN gives "unordered", which C++ operators express differently. S2.1 converts with explicit helpers, not ad-hoc `<`/`>`.
5. **Memory ordering.** x86 orders stores strongly; Apple silicon doesn't. Cross-thread flags and hand-offs that rely on `volatile` or on x86 ordering (for example `VoipServer/BF2VoipServerShell.cpp`'s `volatile LONG g_stop`; 33 files use `volatile`, most for other reasons) become `std::atomic`. The `Interlocked*` mappings (17 files) use sequentially consistent `__atomic` builtins. A ThreadSanitizer run on the M2 checks the result.
6. **`wchar_t` is 4 bytes on macOS** (2 on Windows), on both architectures. Code that reads UTF-16 text from game files with `sizeof(wchar_t)` (`IO/File/File.cpp`, `IO/Localization/ExcelLexicon.cpp`) breaks on macOS. On-disk and on-wire text uses `char16_t`, converted at the boundary. 175 files use `wchar_t`; only the file/network boundaries need changing.
7. **SSE and `cpuid`.** No engine code uses SSE intrinsics; only `External/imgui`, `External/libvorbis` (both have portable paths) and two tests do. `RendDX9/VideoOptions.cpp` calls `cpuid`; on `arm64` it gets an arm reporting the CPU via `sysctl`.
8. **Alignment and packed data.** `arm64` allows unaligned ordinary loads but not unaligned atomics. Structures read from files or the network (`#pragma pack` in 3 files, `BitStream`) are read field by field or with `memcpy`, never through atomics.

## Display (S2.4 and stage 3)

The retail client asks for classic fullscreen modes (800×600 at 85 Hz on first start). Apple silicon MacBook panels don't offer them; under Wine that was the cause of "no window at all" on the M2, and any non-native mode gave black bars and offset mouse clicks. The native client therefore:

- never switches the display mode: borderless fullscreen or a window at the drawable size of the screen;
- renders at an internal resolution chosen in the video options and scales to the drawable (`CAMetalLayer.contentsScale`, Retina); the resolution list offers fractions of the native size instead of enumerated modes;
- keeps mouse coordinates in drawable space, so clicks land where the cursor is at any scale;
- treats the notch area of recent MacBooks as outside the safe area for HUD and menus;
- uses the display's refresh (60 Hz, ProMotion 120 Hz) through `CAMetalLayer`, with vsync on by default and switchable.

## Stage 3 outline (Metal)

1. Inventory the D3D9 calls actually made inside the renderer image, and define a narrow internal device interface: resources, lock/unlock, render targets, state, vertex declarations, draws, queries.
2. Implement that interface with Metal (Objective-C++), and keep a D3D9 implementation on Windows.
3. Effect framework: techniques, passes, state assignments and parameter binding, with the Effect parameter names the engine sets.
4. Shaders: translate the 101 `.fx` effects to MSL. Choosing a translation tool needs the owner's OK before any install. Shaders are compiled at runtime with `newLibraryWithSource`, because full Xcode is not installed.
5. Replace the D3DX helpers: texture loading (DDS DXT1/3/5 → BC1–3), mesh utilities, math.
6. First target: one map renders with terrain, static meshes and the sky. Particles, decals, overgrowth and post-effects follow.

Design rules for Apple GPUs (tile-based) that also run well on the Intel Mac's GPU:

- **Pipeline states:** D3D9 sets states freely; Metal bundles them into pipeline objects. The device seam keeps a pipeline cache keyed by the state combination, fills it while a level loads (not at the first draw), and keeps compiled pipelines across runs with `MTLBinaryArchive`. Otherwise the first use of a material stalls the frame; first-use stalls were visible under Wine too (about 2 s per new map preview in the menu).
- **Dynamic buffers:** `Lock` with `D3DLOCK_DISCARD` (13 sites) and `D3DLOCK_NOOVERWRITE` (2) maps onto per-frame ring buffers in shared memory (`MTLStorageModeShared`). Apple silicon has unified memory, so no staging copies; on the Intel Mac's integrated GPU the same path works.
- **Render passes:** set load and store actions explicitly (don't load what gets cleared, don't store what isn't read later). Depth and MSAA buffers that live only inside one pass are `memoryless` on Apple silicon. The renderer switches render targets in 19 places (shadow maps, glow, reflections); group the work so each target is one pass per frame where possible.
- **Precision:** shaders use `float`; `half` only where measured and visually checked.
- **Textures:** BC1–3 are supported on both Macs (check `supportsBCTextureCompression` at start-up and fail visibly otherwise).
- **Validation:** every renderer milestone is checked on both GPUs (Intel Iris Plus 655 and M2), with the Metal validation layer on in debug builds.

## Performance target (stage 4)

The benchmark from the Wine investigation stays the reference: single player, Strike at Karkand, spawn at the US main base, no input, 60 s average FPS from the engine's own counter, vsync off. Baselines: M2 under Wine/Rosetta about 17 FPS (hardware audio, EAX off), Intel Mac retail 60 FPS (vsync cap, reported, not yet measured with this benchmark).

- Target on the M2: at least 60 FPS at the native "looks like" resolution with default settings, and no hitch above 100 ms when opening a menu map preview.
- The game and render threads run with `QOS_CLASS_USER_INTERACTIVE` (so macOS keeps them on performance cores), loading threads with `QOS_CLASS_USER_INITIATED`; audio already uses `USER_INTERACTIVE`. `Core/Thread.h` gets the mapping.
- The app declares itself a game (`LSApplicationCategoryType` = `public.app-category.games`, `LSSupportsGameMode`), so macOS Game Mode applies.
- Memory: the 8 GB M2 is the minimum test machine. Peak memory per map is measured there; 64-bit pointers make the engine's structures larger than in retail.

## Rules for every package

- Follow AGENTS.md. Port code is `[CHOSEN, not recovered]`, and fidelity code is not changed without evidence. Never fake success: use a `[FRONTIER]` log, then abort.
- Agents commit only on their own `mac/<pkg>` branch. Nobody pushes or opens a PR; the owner reviews and pushes.
- MSVC and Linux builds can't be run on this Mac. Every change to shared code is kept behind platform macros and listed, so the owner can check those builds.
- `arm64` results are reported separately from `x86_64` results; "links for arm64" is not "runs on arm64".
- Server tests use port 16890 inside the `sandbox-exec` profile in `out/smoke/`, never port 16567, and stay offline.
- Game content (the Wine bundle in `~/Applications`) is read-only. Tests work on APFS clones under `out/`.
- Installing software (OpenAL Soft, shader translators, Ninja, Xcode, the Command Line Tools on the M2) needs the owner's OK.
