# Native Linux Vulkan client

The intentional design is tracked in [issue #36](https://github.com/kr4uzi/OpenBF2/issues/36).
The target is Steam Deck desktop, vanilla multiplayer and local bots, with Linux ARM64
included in the build design. Windows and macOS remain separate backends. All new port
policy is `[CHOSEN, not recovered]`; this work establishes no new retail recovery claims.

## Current reach

The native Linux x86-64 client runs on Steam Deck desktop. The owner accepted the observed
Operation Clean Sweep local-bot playtest after the terrain correction, and confirmed the
menu's overall appearance and corrected cursor. Console appearance was compared with retail;
both screenshot paths now produce opaque PNGs. The renderer shutdown crash reproduced before
the upstream PR #37 fix, and both in-map QUIT and return-to-menu QUIT passed afterward.
The validation record below identifies the runs, limitations and supporting fixtures.

The standalone window/presentation check was also visually accepted. It exercises the same
reduced D3D9 interfaces through typed DXVK Native forwarding: drawing, resize/reset,
fullscreen, focus, relative mouse capture and screenshots. That boundary check is separate
from the full-client playtest. Multiplayer, reconnect, map changes, longer post-fix stability,
physical controllers, Gaming Mode, audible menu acceptance and a full ARM64 client remain
unvalidated.

The engine-facing subset and the full DXVK SDK are separate types. The boundary verifies
structure size/alignment, enum values and fixed-width scalar types against the installed
SDK. `D3DFMT_FORCE_DWORD` is an unused enum-size sentinel with different values in the two
headers; its value is excluded, while the enum size remains checked. D3D resources keep
their reference counts and wrapper identity when queried through supported base interfaces.
The native SDK's stable IID objects are used instead of retaining references to temporary
`__uuidof` results. The forwarding library stays loaded for the process lifetime.

The shared generator has a separate `--d3d9-only` output directory. D3DX does not forward
to a Windows DLL. The shared effect compiler, CPU runtime and D3DX effect facade now build
on Linux, and an SDL/DXVK device check validates their rendered output. Portable D3DX helpers,
SDL input/windowing, static plugin isolation, audio/video, Python/AI assembly and protected
profile data are integrated. The shared input-script interpreter drives the SDL host and
requests labelled PNG captures. Human-authored `mark`/`goto` scenario extensions remain planned
work. Existing recovery
frontiers are unchanged. No IDA/Ghidra session was accessible during this work; any new
recovery or unknown ABI contract still needs direct original-image evidence.

## Local review scope

The Linux port starts with five commits on `linux/vulkan-client`, based on
upstream `main` at `02cc5ac818582689efe3452755f40e5f55edc2c7`: numeric portability;
SDL host, input, profiles and plugin integration; audio/video; DXVK and portable
D3DX/effects; then build, launch and validation tooling. These are a dependency-ordered
review series; full-client validation applies to the complete series.

A separate host/input refactor follows those commits. It gives shared code neutral names and
removes Linux's conversion through Mac key codes. The host architecture below describes its
internal API changes and the intended platform-specific behavior.

Use `git diff origin/main...HEAD` for the proposed changes and
`git log --reverse --oneline origin/main..HEAD` for the review order. The worktree is
committed, so plain `git diff` is empty. Generated native forwarding files can be
reviewed alongside `tools/macos/gen_d3d9_forward.py`; they are reproducible source,
not checked-in build products. Runtime libraries, shader-equivalence manifests,
reference content, benchmark outputs and screenshots remain under ignored `out/`.

Upstream's quit fix, widescreen HUD/FOV policy, water-buffer guard and BFHQ groundwork
are inherited from the base rather than submitted again. The quit fix has no remaining
Linux-specific production hunk. The native host still uses the existing renderer
interfaces; no networking protocol or retail content-format change is introduced here.
Windows/macOS and full Linux ARM64 build/runtime checks remain outstanding, as do
the broader gameplay acceptance cases listed below.

## Build the presentation check

Use a 64-bit Linux C++ compiler, CMake 3.21+, Ninja, Python 3, SDL3 development files and
an architecture-matched DXVK Native prefix. Configure-time linking checks the latter;
it does not run DXVK. The tested dependencies are SDL 3.4.20 and DXVK Native 3.1.1:

| Source archive | SHA-256 |
| --- | --- |
| [SDL3-3.4.20.tar.gz](https://github.com/libsdl-org/SDL/releases/download/release-3.4.20/SDL3-3.4.20.tar.gz) | `f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f` |
| [dxvk-native-3.1.1-steamrt-sniper.tar.gz](https://github.com/doitsujin/dxvk/releases/download/v3.1.1/dxvk-native-3.1.1-steamrt-sniper.tar.gz) | `506fa1ea5bb6ce9a89e11436f7ab5191bc0196e8a577e0cf0b16a45bbc9991b7` |

Extract dependencies into a private build directory. `BF2_DXVK_ROOT` names the extracted
`usr` directory, containing `include/dxvk` and `lib`. Build/install SDL into a private
prefix, or use your distribution's SDL3 development package:

```bash
cmake -S /path/to/SDL3-3.4.20 -B out/sdl-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/absolute/path/to/sdl-prefix \
  -DSDL_TESTS=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_STATIC=OFF
cmake --build out/sdl-linux --target install -j4
cmake -S Code/Tests/linux_vulkan -B out/linux-vulkan-window -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBF2_DXVK_ROOT=/absolute/path/to/dxvk/usr \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/sdl-prefix
cmake --build out/linux-vulkan-window -j4
ctest --test-dir out/linux-vulkan-window --output-on-failure
```

The root engine configuration can also expose these targets with
`-DBF2_LINUX_VULKAN_CHECKS=ON` and the same dependency settings. It defaults to OFF.
Every new engine TU sets `DEDICATED=0` and force-includes `BuildConfig.h` in the
graphics target; recursive dedicated/Windows discovery sees inert guarded TUs.

The initial presentation-only SDL build used `out/linux-vulkan-deps/sdl` and the existing
`mingw-build` Distrobox. It enabled X11, disabled Wayland and XScreenSaver support because
their development dependencies were unavailable, and had no real audio backend. That early
dependency configuration does not describe the later full-client runtime: its staged SDL
and OpenAL libraries use the desktop PulseAudio service, as described below. Native Wayland
and Gaming Mode behavior remain unvalidated.

## Native effects

Linux reuses `Platform/Gfx/Effect` and the D3DX facade in `Platform/Gfx/Metal/Effect.cpp`;
the latter's directory name is historical. vkd3d compiles HLSL to D3D9 bytecode, and
MojoShader reflects constants, samplers and preshaders without requesting Metal output.
The dependency build explicitly disables the Metal profile. The engine keeps C++14;
these isolated effect libraries retain their existing C++17 requirement.

Linux uses OpenSSL's SHA-256 and a distinct compiler identity. The effect cache defaults
to `$XDG_CACHE_HOME/openbf2/FxNative` (when the XDG path is absolute), otherwise
`$HOME/.cache/openbf2/FxNative`; `BF2_FX_NATIVE_CACHE` overrides it. Imported native `.cfx`
request envelopes are retargeted to the current compiler identity before lookup.
The original file formats and gameplay protocol are unchanged. macOS keeps CommonCrypto,
Metal translation and its existing cache location.

Use the same pinned sources as the macOS compiler, with the shared repository patches:

| Source archive | SHA-256 |
| --- | --- |
| [vkd3d-2.1.tar.xz](https://dl.winehq.org/vkd3d/source/vkd3d-2.1.tar.xz) | `7510146aff2adfb4ae07ab890701a607e5ff7c66e57100cfc9f630ec92eeda6a` |
| [MojoShader ad5dff8](https://github.com/icculus/mojoshader/archive/ad5dff84830c2863c841f4b1f4e3df78c705b383.tar.gz) | `728ff9098115059c4012895b7a85065688c857fdadf9fbc04484fec8c7be3081` |

Build prerequisites are a native Linux C/C++ compiler, CMake, Ninja, make, bison, flex,
patch, Python 3.9+, pkg-config, WIDL, Perl JSON, Vulkan/SPIR-V headers and OpenSSL development
files. The vkd3d release archive already includes `configure`; a Git checkout also needs
autoconf, automake and libtool to run its `autogen.sh`. No source downloads or package
installation occur in this script:

```bash
python3 tools/linux/build-nativefx.py \
  --vkd3d-source /absolute/path/to/vkd3d-2.1 \
  --mojo-source /absolute/path/to/mojoshader-ad5dff8 \
  --output /absolute/path/to/nativefx-linux --jobs 4
cmake -S Code/Tests/linux_fx -B out/linux-effects -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DBF2_FX_NATIVE_DEPS=/absolute/path/to/nativefx-linux
cmake --build out/linux-effects -j4
ctest --test-dir out/linux-effects --output-on-failure
```

The builder copies and patches the supplied sources in its own output directory. It
records source/patch hashes, compiler version and flags, and refuses reuse with different
inputs. Choose a new output directory when any of those change. vkd3d uses native objects
with LTO disabled so the C archives do not require the same GCC LTO version as the consumer.
CMake verifies dependency compatibility by linking, including during cross compilation.
The builder itself is native: run it on an ARM64 Linux system for ARM64 dependencies.

Optional native shader reuse uses locally audited full/trimmed compiler equivalence. It is
independent of D3DX acceptance, shader-profile promotion and deferred effect publication.
Build `bf2_fx_equivalence` in the same configuration as the client, then give it a newline-separated
list of captured `.request` files, an output manifest path and a worker count from 1 to 8:

```sh
cmake --build out/build/linux-client --target bf2_fx_equivalence
out/build/linux-client/Code/Tests/linux_fx/effect/bf2_fx_equivalence \
  /absolute/path/to/request-list.txt /absolute/path/to/equivalence.manifest 2
cmake -S . -B out/build/linux-client \
  -DBF2_FX_EQUIVALENCE_MANIFEST=/absolute/path/to/equivalence.manifest
cmake --build out/build/linux-client --target bf2_client bf2_linux_client_checks
```

The generator compiles both inputs independently, refuses mismatches, omits refused jobs and
writes an audit beside the manifest. Use captured requests from a successful compilation run;
this step neither downloads game assets nor establishes retail compiler acceptance. Generated
evidence stays in a private output directory. The configured manifest is embedded in the build;
clear `BF2_FX_EQUIVALENCE_MANIFEST` to disable it. Exact full input, compiler fingerprint and
output bytecode hash must match before reuse. Absent, stale or mismatching evidence and failed
optional attempts fall back to full compilation. Successful bytecode storage is limited to
32 MiB and 4,096 entries; retained payload and reuse counts are logged at shutdown. Compiler,
dependency or configuration changes require another audit. Changing the manifest also changes
the persistent effect-cache identity, without changing the audit fingerprint itself.

The root configuration exposes the CPU checks with `-DBF2_LINUX_EFFECT_CHECKS=ON` and the
same dependency path. To also build the manually run GPU check, add
`-DBF2_LINUX_FX_DEVICE_CHECK=ON`, `-DBF2_DXVK_ROOT=...` and your SDL3 prefix. Then run:

```bash
out/linux-effects/bf2_linux_fx_device /absolute/path/to/private-test-effect-cache
```

This opens a hidden 64×64 SDL window, creates a real D3DX effect, binds a texture, changes
its color parameter twice, reads the resulting pixels back from DXVK and checks render-state
restoration. It also checks an imported request bearing another backend's identity.
It requires a working graphical session and Vulkan driver; it is deliberately outside the
default CPU CTest suite. No game content, server or socket is involved.

The existing `BF2_HITCH_TRACE` instrumentation also records Linux effect requests,
compiler waits and device shader creation. Its Metal-specific categories remain unused;
these timings do not claim GPU timestamps or DXVK pipeline timing. The shared warm-up
counters build with the facade, but the client-level warm-up flow is not wired on Linux yet.

Validation on this checkout: all four combined CPU/forwarding CTests pass, including
169 shared runtime assertions, both standalone with GCC 14.2 and through the root build
with GCC 16.2. The root `bf2` strict link and `dice_py_check_run` also pass after integration.
The GPU check passes on RADV VANGOGH 26.2.5: expected BGR
pixels `(191, 128, 64)` and `(64, 32, 191)`, with state restored. Separate cold and reused
effect-cache runs pass; their request trace reports compilation then a cache hit. DXVK
reported an unreadable shader-cache header in the reuse run, so that run establishes only
effect-cache reuse, not a fully warm GPU cache. These initial checks preceded the full-client
menu and local-map validation recorded below.

The shared CPU fixture's obsolete abort expectation was updated to the current documented
`Refused` exception contract. The production refusal behavior is unchanged.

## Build and run the full client

The `linux-client` preset uses a native x86-64 or ARM64 compiler. Supply architecture-matched
DXVK Native and the effect compiler dependencies described above:

```sh
cmake --preset linux-client -B out/build/linux-client \
  -DBF2_DXVK_ROOT=/absolute/path/to/dxvk-prefix \
  -DBF2_FX_NATIVE_DEPS=/absolute/path/to/nativefx-linux
cmake --build out/build/linux-client --target bf2_client -j 6
cmake --build out/build/linux-client --target bf2_linux_client_checks -j 6
```

`tools/linux/Containerfile` provides the build packages. Engine and portable helper TUs remain
C++14; the existing isolated effect compiler/runtime targets use C++17. The executable,
`libdice_py.so` and `libaidll.so` are written to the build's `bin_linux` directory.
`bf2_client.build.json` records the linked binary hash, compiler, architecture and source state.

Linux uses `host::ClientHost` with an SDL implementation. Cocoa implements the same contract
on macOS; Linux never defines `__APPLE__`. Native ELF plugins are prelinked with their own host-service
globals. Private COMDAT signatures are renamed too, so plugin destructors and RTTI cannot be
coalesced across images. A host-plus-two-plugins runtime fixture checks that isolation.

### Shared host and input

`Platform/Host/ClientHost.h` defines the application, window, cursor, clipboard and input
contract in `dice::hfe::host`. `CocoaHost.mm` and `SDLHost.cpp` provide its private backend
implementation. `rendererWindowHandle()` borrows the content `NSView*` on macOS or the
`SDL_Window*` on Linux. Cocoa-only test hooks and native inspection stay in the macOS backend.

Host keyboard events carry BF2's existing DIK physical-key codes; UTF-16 text arrives separately.
SDL scancodes map directly to DIK. `MacKeyMap` handles only Apple kVK/ISO/JIS conversions,
including the existing Mac F13–F15 policy. Shared key metadata supplies Win32 message codes
and fallback labels, with Option/Command/Clear overrides on macOS. Layout character lookup
accepts DIK, so shared devices and menu code never translate through Apple key codes.

`NativeInputDeviceManager`, `NativeKeyboardDevice`, `NativeMouseDevice` and
`NativeGameControllerDevice` retain the engine interfaces, factory IDs and sampling behavior.
The shared window-message adapter consumes normalized events. Cocoa normalizes its Backspace,
keypad Enter and Command shortcuts before dispatch; Ctrl+C remains U+0003, not Enter. Linux
uses Control for shortcuts; Super does not act as Command. Saved bindings and named input script
commands keep their existing meanings. Numeric test-script keys are backend-specific: Linux
`key 0xNN` now names DIK directly, while macOS retains kVK. Use named keys in portable scripts;
none of the repository's Linux scripts relies on the former numeric Mac tokens. Linux text
expansion searches all 256 DIKs, including printable extended keypad keys. These are intentional
native host changes under issue #36; no recovered layouts, network protocols or content formats change.

Host/input refactor validation:

- GCC 14 client strict link and integrated checks: 33 passed, one existing binary128 check
  skipped on x86-64. `linux_host_script_layout` exercises the real interpreter with a synthetic
  keyboard layout whose printable characters exist only at DIK 141, 179 and Shift+181.
- Native dedicated-server and `dice_py_check` targets build with the existing Distrobox
  toolchain. This check does not claim a new dedicated-server runtime or Python runtime test.
- The portable keymap/Cocoa-text/window-message fixture passes as an AArch64 binary under
  QEMU (GCC 14.2, QEMU 10.0.13). This establishes CPU-boundary coverage, not ARM64 graphics.
- Client run `20261116T181350Z-6e760c6c` loaded Operation Clean Sweep, spawned, typed and edited
  a console command, used keypad Enter, walked and turned, disconnected and confirmed the
  game menu's Quit. It exited 0 after 76.19 seconds, with 2,187 traced frames and 781,512 KiB
  peak RSS. It used copied content on port 16698, 1280×800, `+restart 1`, the saved eight-sample
  setting and opt-in scripted input. Console and movement captures were inspected. Concurrent
  compilation makes this a functional check, not a performance comparison. The subsequent
  extended-key search change is covered by the interpreter regression above.
- macOS AppKit/Metal and MSVC x86/x64 builds remain pending. Live keyboard-layout variations,
  UI rebinding, physical-controller behavior and a new human focus/capture review were not
  exercised. Synthetic tests cover the shared focus and captured-mouse message rules.

Logs are `out/linux-vulkan-client/host-refactor-final-checks.log` and
`out/linux-vulkan-server-check/host-refactor-distrobox-build.log`; ARM64 fixture artifacts are
under `out/linux-audio-agent/native-host-arm64/`. Host test hooks retain
`macos-client-window`; scripted input retains `client-test-automation-replaced`, and Cocoa
activation/fullscreen workarounds retain `macos-options-validated`. This refactor retires none
of those boundaries.

GCC builds disable devirtualization for the reconstructed native images. Their explicit-self
tables are viewed through current C++ interfaces; GCC otherwise optimizes a fixture's call into
`__cxa_pure_virtual`. The native ABI and saved-dispatch checks gate the client build.

Profiles and the instance lock use `$XDG_DATA_HOME/OpenBF2`, falling back to
`$HOME/.local/share/OpenBF2`. Protected passwords/CD keys retain the existing native `OBF2`
version-1 encrypted blob format: AES-256-CBC with padding and HMAC-SHA256, with a random
64-byte key in `ProtectedData.key` (mode 0600). Linux uses OpenSSL and preserves the native
decode-failure path for Windows DPAPI blobs. Tests use synthetic secrets and isolated directories.

Run only against a separate content copy for validation. The current local copy is
`out/linux-vulkan-content`; it includes `mods`, `python`, `Settings`, `pylib-2.3.4.zip` and
`00000000.256`, and omits the owner's profiles and Windows binaries. Its
`.openbf2-linux-test-content.json` records the source and test port; the copied
`mods/bf2/Settings/ServerSettings.con` selects port **16697**. The launcher checks that this
port is unused, checks any saved default-profile server configuration, and passes it explicitly
as `+port`; it never permits 16567. The original installation stays read-only.

```sh
python3 tools/linux/client.py out/build/linux-client/bin_linux/bf2_client \
  --content out/linux-vulkan-content \
  --state out/linux-vulkan-client-state \
  --purpose 'Menu: check labels, navigation, clipping, colors, input alignment and audio'
```

`--runtime /absolute/path/to/private/lib` adds a matching runtime library directory. The local
Deck validation uses `out/linux-vulkan-runtime/lib`, containing the build's SDL3, JPEG and
OpenAL libraries plus missing dependencies. `tools/linux/stage-runtime.py` records their
package versions and hashes; it does not replace the system's libc, C++ runtime or graphics
driver. This local Distrobox runtime uses `SDL_AUDIODRIVER=pulseaudio` and
`ALSOFT_DRIVERS=pulse`; playback-device checks passed against the desktop service without
opening a microphone.

Each launch prints its directory under `out/linux-vulkan-client-runs`. F12 queues a PNG in
that run's `captures` directory. The renderer saves the actual swap-chain backbuffer before
Present and records success/failure and readback time in `captures.jsonl` and the hitch trace.
The frame summary excludes capture frames from its ordinary frame-time distribution.
`--seconds N` bounds a diagnostic run; its timeout is recorded and does not count as normal
shutdown. `--warm-cache PATH` reuses a previous cache, and `--detail detailed` enables the
DXVK diagnostic overlay. GPU timings and unmeasured upload/wait attribution remain unknown.
`--skip-intros` selects the existing `+restart 1` path for repeated menu diagnostics. That
path also skips intro-driven Bink sound initialization, so it cannot validate movie audio;
use a normal launch for audio acceptance and keep the two startup paths separate in comparisons.
`--singleplayer-map Operation_Clean_Sweep` requests the existing command-line local-bot path
with `sp1`, 16 players and the saved default offline profile. It requires that map's copied
`server.zip` to contain `GameModes/sp1/16/GamePlayObjects.con`, and retains the validated test
port. It does not change saved video settings. A first manual offline login is required to
create the local profile; the launcher does not forward account passwords.
The launcher owns the client process group and cleans up descendants on exit or interruption.
Its runs describe one client process; a mod-switch relaunch needs a separate validation run.

The menu acceptance check covers readable text, navigation and settings, image colors and
blending, input alignment, background/button audio, transitions and saved captures. Startup,
compilation and test success do not grant this acceptance.

## Human window and presentation review

From this checkout, launch the already-built check in the existing container:

```bash
distrobox enter mingw-build -- python3 tools/linux/presentation.py \
  out/linux-vulkan-window/bf2_linux_presentation \
  --purpose 'Moving RGB triangle and all four white edges stay visible; resize, fullscreen and focus must not freeze or offset input.' \
  --power-note 'Record plugged-in/battery status, power limit and display refresh rate'
```

This test uses no game content, networking or server ports. It does not touch the reference
installation or the widescreen work under `out/hud-widescreen`. All later client/server
tests must use copied content roots and an explicitly unused port other than **16567**.

Review these checks manually:

1. Confirm the initial 1280×800 client area, a moving red/green/blue triangle and white
   markers on all four edges. Look for clipping, incorrect colors, tearing and flicker.
2. Resize repeatedly. Confirm the markers stay at the new edges and animation stays responsive.
3. Press **F11** to enter and leave fullscreen; minimize/restore and switch focus away/back.
4. Press **F6** for relative mouse capture. Losing focus releases capture; F6 enables it again.
   Confirm the pointer returns normally after release and exit.
5. Press **F12** at each problematic state to save a BMP, then **Escape** to exit.
6. Record observations with the run ID and screenshot frame number. Human acceptance remains
   pending for a new build until the visuals and timing evidence have been reviewed together.

Failures of the device, reset, screenshot or presentation return a nonzero process status.
A bounded `--frames N` run only bounds the test; it never grants acceptance.

## Standalone presentation captures and profiling

Each invocation creates `out/linux-vulkan-runs/<UTC timestamp>-<unique id>/`, containing:

- `run.json`: binary hash, source commit/dirty status, architecture, launch times, settings,
  relevant environment, cache state, driver/power notes and process result.
- `frames.csv`: shared SDL timestamps, frame intervals, CPU draw/present/readback durations,
  screenshot markers, dimensions and process peak RSS. Present duration is a CPU measurement,
  not GPU time. RSS is a high-water value, not current memory use.
- `events.log`, DXVK logs and `frame-<number>.bmp` screenshots, correlated by run ID,
  frame number and the same SDL clock. This clock begins at SDL initialization.
- `summary.json`: interval distributions and hitch rows. Startup and intervals containing
  screenshot readback are excluded from the ordinary interval summary but retained in the raw CSV.

The default light run has no DXVK HUD. `--detail detailed` enables the HUD's device, timing,
pipeline/compiler and resource counters. Compare it with an otherwise equivalent light run;
the capture loop itself also has measurement overhead. `--warm-cache <previous-run>/cache`
reuses that cache; without it both DXVK and Mesa receive a new empty cache directory.
Reused cache contents are recorded as unverified, not assumed fully warm.

GPU timestamps, CPU call stacks, upload timing and synchronization attribution remain unknown
in this initial check. For a detailed GPU investigation, capture the same executable under
RenderDoc using the run's recorded command, directory and environment, then repeat without
RenderDoc. Record the external trace filename and run ID. The local investigation below used
RenderDoc 1.46; it is not a build dependency. A visual finding should record the observed defect, associated
timing/resource evidence, suspected cause, proposed change and repeatable verification.

## Full-client captures and profiling

`tools/linux/client.py` writes a separate `out/linux-vulkan-client-runs/<run-id>/` directory.
Its `run.json` identifies the executable, source/build state, invocation, copied content,
isolated profile location, cache and instrumentation settings, duration and exit reason.
`client.log` contains startup and runtime diagnostics; `bf2_client_d3d9.log` records DXVK's
selected device, driver and presentation parameters.

The manifest snapshots allowlisted saved `VideoSettings` commands before and after each run,
separately from requested dimensions and actual DXVK presenter evidence. Saved settings do not
prove which settings were applied. Available sysfs power fields include AC/battery state, CPU
governor, GPU policy and power caps with units; missing fields stay unknown. The launcher reads
no account/password files. Graphics override configuration is identified by hashes, and cache
file counts/sizes describe contents without asserting that every shader or pipeline is warm.

`hitches.log` uses the shared hitch tracer's steady clock, beginning at tracer initialization.
Its `F` rows record frame intervals, in-game status and six main-loop wall-time sections
(pre, update, script, objects, render and tail). `R`/`B` rows describe effect requests/builds;
`N` rows correlate script labels and capture events. These are CPU wall durations, including
waits; they are not GPU timings or sampled CPU stacks.

PNG files and `captures.jsonl` live under `captures/`. The capture record states the Present
counter, label, success or failure and readback/encoding duration. `summary.json` excludes the
first interval and capture intervals from its ordinary distribution while retaining all
intervals over 50 ms. It does not remove startup transitions after the first frame or imply
that a run measured steady-state gameplay. Human observations should cite the run ID and
capture filename. A timed diagnostic exit, normal quit and user interruption are distinct
fields in `run.json`.

## Linux ARM64

The graphics boundary has no x86 calling-convention or pointer-width assumptions; it requires
a 64-bit Linux toolchain and checks native D3D layouts. On Linux ARM64 use the same configure
command with ARM64-built SDL3 and DXVK Native. The downloaded Steam Runtime DXVK binary is
x86-64 and must not be reused on ARM64. For a source build, the pinned DXVK release exposes
`native_sdl3`, `native_sdl2`, `native_glfw` and the `enable_d3d*` Meson options; see its
[build definition](https://github.com/doitsujin/dxvk/blob/v3.1.1/meson.build) and
[options](https://github.com/doitsujin/dxvk/blob/v3.1.1/meson_options.txt).

Linux client images select the existing numeric adapters with `BF2_NATIVE_X87`. Linux ARM64's
binary128 `long double` selects software 80-bit arithmetic, as Apple ARM64's binary64 does;
using the host type directly would change intermediate rounding. Native/software x87
differential and BitStream boundary checks pass on x86-64, including format-selection checks
for both binary64 and binary128. The binary128 constructor now converts directly into software
x87 precision instead of first narrowing to `double`. GCC 14 AArch64 binaries running under
QEMU passed 33,344 GCC reference conversions across all four rounding modes, direct conversion
boundaries, numeric/BitStream checks and the native saved-dispatch/vtable fixtures. These are
focused cross-build and emulation results. No full ARM64 client/dependency bundle, physical
ARM64 graphics runtime or gameplay has been validated. Reusing macOS ARM64 code does not prove Linux ELF loading, plugin ABI,
graphics-driver behavior or gameplay. Do not describe this checkpoint as tested ARM64 support.

## Validation record

Validation on Linux x86-64 with GCC 14.2 (full client) and GCC 16.2 (standalone checks/server):

- C++14 strict link of `bf2_linux_presentation` and `bf2_linux_forward_contract`: passed,
  both standalone and through the root CMake option.
- Generated native source consistency and SDK ownership/forwarding checks: passed.
- Isolated `linux-x64` build of `bf2`, native `libaidll.so`, `libdice_py.so` and
  `dice_py_check`, with `BF2_LINUX_STATIC_RUNTIME=OFF`: passed. The Python check ran
  successfully (40 builtins, pointer roundtrip, zlib and ephemeral loopback UDP).
- Python syntax, capture-summary percentile checks and `git diff --check`: passed.
- Full native `bf2_client` strict link: passed. Of 32 integrated Linux CTests, 31 passed and
  the binary128-only conversion check correctly skipped on x86-64. These cover
  ABI dispatch, plugin isolation, forwarding, effects, D3DX, host/script input, audio lifecycle
  and refusal paths, protected profiles, mixed-case asset archives, launcher lifecycle and
  native/software numeric boundaries, runtime image geometry, cursor visibility and audited
  shader reuse/fallbacks, AA probe assembly goldens and packed effect colors. The binary128 check passed
  separately under AArch64 QEMU.
- Real-device D3DX checks passed for managed/default textures, cube/volume mip transfers, BC1,
  multisample resolve, PNG/JPEG roundtrips, render-to-surface, mesh normals and line pixels.
- Existing numerical suites with the Linux native selection passed: 1,120,088 native and
  1,120,088 software-x87 checks; 651 portable checks per mode; 11 BitStream boundary checks
  per mode. These ran on x86-64 and do not establish ARM64 execution.
- Audio validation decoded the 3,396-frame menu movie and compared output with FFmpeg; movie
  audio comparison reached 135 dB SNR. Lifecycle/failure tests also passed under AddressSanitizer.
  Both menu movies' frames 1, 30 and 100 matched FFmpeg's decoded YUV exactly and produced
  varied grayscale RGB images. Direct Bink file opens now share the Linux case-insensitive
  path resolver, fixing skipped `Dice.bik`, `Legal.bik` and `Intro.bik`. The manual
  `bf2_linux_bink_path_check` target passed normal and AddressSanitizer runs against a supplied
  movie: case/separator variants decode identically; missing, ambiguous and invalid files fail.
- Full-client run `20261115T230948Z-cec6bcc2` reached the welcome/menu draw loop and recorded
  4,374 frames. Its 60-second diagnostic timer ended the run; exit 0 is not a normal-quit proof.
- Separate clean-profile run `20261115T231801Z-63f7442c` captured the welcome screen and quit
  through the shared script: 9.94 seconds, 552 frames, exit 0 without timeout. Its PNG is
  `captures/present-00000514-menu-start.png`. Capture readback/encoding took 137.15 ms; frame
  514 is excluded from the ordinary frame distribution. Median interval was 10.00 ms and
  p99 12.98 ms. The run includes startup/movie transitions, with a 609.70 ms interval after
  the first frame; this is not a steady-menu or gameplay benchmark. Peak process RSS was
  255,284 KiB. Visual acceptance was pending at this stage.
- Repeating startup with saved high-quality settings exposed an `FSQuadDrawer` technique
  failure: the shared validator incorrectly required Metal output on Linux. It now requires
  MSL only on Metal. A real-device regression fails against the preserved pre-fix archive and
  passes after the fix, including fresh/cache/imported techniques, pixels and state restoration.
- Saved-profile cold run `20261115T233839Z-6e4f153a` and reused-cache run
  `20261115T233945Z-1816476f` both captured the login screen and quit normally. They recorded
  21 compiled effects and 21 effect-cache hits respectively; the previous DXVK assertion did
  not recur. Cold: 10.54 s, 552 frames, median 10.01 ms, p99 14.78 ms, peak RSS 290,464 KiB.
  Reused: 9.53 s, 612 frames, median 10.00 ms, p99 13.91 ms, peak RSS 177,512 KiB. The largest
  non-capture interval after the first frame fell from 600.84 ms to 98.66 ms. These short runs
  include startup/movie transitions. Sysfs reported AC connected, CPU powersave, GPU auto and
  15 W caps; GPU execution timings remain unknown. Human menu acceptance was pending at this stage.
- Scripted offline-account selection in `20261115T234023Z-4ec01ca4` reached the main menu,
  captured it and quit normally after 811 frames, but exposed a uniformly gray movie background.
  The movie and other dynamically loaded images generated no geometry because the rectangle's
  `m_new_shape` flag was uninitialized. The Linux caller now initializes it to false under the
  issue-discussed intentional policy; the recovered constructor and other platforms are unchanged.
  The production `loadMovie` regression fails before the fix and passes afterward. The existing
  `swf-runtime-image-owner` lifetime boundary is retained.
- Forwarded D3D textures do not share C++ RTTI identity with the engine-facing subset types.
  D3DX helper type queries now use `GetType` and the public texture interfaces. This fixes default
  mip generation for loaded menu DDS images. GPU regressions cover 2D fill, all cube faces,
  volume filtering and generated BC3 DDS mips; actual copied `blank.png`/`playNow.png` assets
  now load successfully.
- Post-fix run `20261116T054851Z-2cc2d829` captured the movie background and loaded Play Now
  image, then quit normally after 315 frames. It used `+restart 1` to skip intros and does not
  validate movie audio. Its image is `captures/present-00000304-main-menu.png`. The normal
  untimed review run `20261116T054930Z-fb43aa12` was opened for the owner, who reported that
  the menu looked good but the normal and busy cursors were missing. Audio acceptance remains
  pending. Earlier review prompts referred to windows that had already closed.
- Cursor loading now resolves physical path casing before opening files. The menu requests
  lowercase paths while the copied assets contain `FlashMenu/Cursor`; failed loads previously
  caused each menu frame to hide the cursor, with mouse motion temporarily showing the system
  pointer. A dummy-SDL regression fails before the fix and passes afterward for normal/busy
  images, hotspots, Windows separators, visibility without motion, explicit hiding and restore.
  This uses the existing issue-discussed Linux asset path policy. The owner confirmed that
  the cursor is visible in `20261116T061149Z-384ff578`; busy animation was not separately reviewed.
- In `20261116T054930Z-fb43aa12`, the owner selected Operation Clean Sweep and reached
  "Optimizing shaders", geometries 62%. The process exited with status 1 after the native
  compiler rejected `vsFullMRTtangent` in `skinnedmesh.fx` for excessive temporary registers;
  the subsequent missing `DrawShadowMap` technique was fatal. It was not a launcher timeout.
  The exact captured source, defines and includes compile with the original D3DX compiler
  in an isolated offline Wine prefix. That comparison is compiler evidence only; Wine's D3D9
  validator does not establish native Windows device validation. Native compiler allocation
  failed because patch 0009 compared uninitialized bytes outside scalar constant values,
  preventing reuse of identical relative matrix loads. Patch 0016 compares only declared
  components at their scalar width, preserving bit distinctions such as signed zero and NaN
  payloads. The failing two-bone shader drops from 33 temporary registers to 10; the one-bone
  variant drops from 32 to 9. Both compile with their requested `vs_1_1` header. They still
  exceed that legacy profile's instruction-slot limit, so compilation alone does not establish
  strict D3D9 profile validity or working DXVK rendering. The run's 5,071 main-loop frame rows
  precede map loading and do not measure the loading loop or gameplay.
- With patch 0016, the complete failed request compiles to 116 shader records and 21 techniques.
  The synthetic relative-load regression fails against the old compiler and passes with the fix.
  DXVK readback also passes with asymmetric matrices and changed index pairs: BGR `(76,62,66)`
  then `(69,55,57)`. This checks the arithmetic and actual device path, without claiming strict
  legacy instruction-slot compliance.
- Owner run `20261116T061149Z-384ff578` completed map loading, entered the game loop and later
  quit normally: 315.30 seconds, 17,323 traced frames, peak RSS 924,436 KiB. The owner reported
  a long shader-optimization pause and terrain transparency/geometry problems. Loading frame
  2149 lasted 123.310 seconds, including 113.158 seconds in effect requests. Of 1,465 requests,
  1,358 compiled synchronously; generated `rashaderbm` effects accounted for 94.408 seconds.
  Request time includes capture, lookup, preprocessing, compilation, reflection and publication;
  it is not pure compiler CPU time. Device shader creation took 0.286 seconds; GPU execution
  and pipeline timings remain unknown. The changed compiler identity correctly made prior
  effect-cache entries cold.
- Reusing the unchanged build/cache in `20261116T062025Z-f4389a44` gave 1,441 cache hits and
  42 cached refusals, zero compilations, and 1.081 seconds in effect requests. Startup plus
  automatic map loading occupied the first 13.268-second frame. It then waited at Join Game
  until the diagnostic timeout; that wait is not loading time. The cold run used manual menu
  navigation and the warm run used `+restart 1` with command-line hosting, so their total
  durations are not a controlled benchmark. Every cold-compiled effect key was a hit on reuse.
- Renderer capture `20261116T062804Z-420677b0/captures/present-00003488-terrain-spawn.png`
  shows a successful MEC Control Center spawn with missing/displaced ground and water visible
  beneath it. The retail game under isolated Proton, with the same map/team/area and port 16697,
  produces solid ground in `out/linux-wine-comparison/retail-game-control-center.png`. Camera
  positions differ; these are diagnostic captures, not a matched pose or accepted visual baseline.
  Pale foliage occurs in both views and is not independently established as a regression.
  Reference installation/profile data were not modified. Subsequent diagnostic launches use
  `+restart 1` at the owner's request; movie-audio acceptance still requires a normal launch.
- A RenderDoc frame from `20261116T071445Z-7d7a1237` preserves the failing scene in
  `out/linux-host-agent/renderdoc/captures/native_frame3189.rdc`. Representative terrain
  vertices agree with the captured shader's position arithmetic within 0.0001221 clip units;
  matching early/final pass vertices are bit-identical. At pixel `(850,360)`, terrain event
  1544 writes color at depth 0.999711 but leaves stored depth at 1.0. Later sky event 1738
  passes at depth 0.99999994 and replaces that color. The earlier terrain pass has no depth
  attachment; the final terrain pass correctly requests no depth writes. The scratch color
  target has one sample while the main color/depth surfaces have eight. DXVK 3.1.1
  `D3D9DeviceEx::BindFramebuffer` omits a depth attachment with a different sample count.
  Pixel history and before/after images are in
  `out/linux-host-agent/renderdoc/pixel-history/`. Capture/replay timings are not load-time
  or gameplay performance measurements. An isolated terrain-quality-1 run also reproduces
  missing ground; the owner's saved quality-3 profile was preserved.
- The native `D3DXAssembleShader` helper omitted the probe vertex shader's `DCL_POSITION0`
  declaration. Original D3DX9_25 emits eight DWORDs, while the helper emitted five. The
  probe therefore read an entirely black image and latched `disableMultisample=1` despite
  the eight-sample device, selecting the incompatible scratch target. The helper now retains
  the original declaration; recovered renderer and terrain code are unchanged. A focused
  old/new DXVK draw/resolve/readback comparison changed from zero to 512,480 nonblack pixels,
  including 2,398 partially covered edge pixels. The rebuilt client's real startup probe
  produces the same histogram and keeps `disableMultisample=0`. The assembly golden fails
  against the old helper and passes after the fix; shared Metal translation remains
  byte-identical, though macOS runtime has not been tested. The general assembler frontier
  remains. See issue #36, `out/linux-d3dx-agent/aa-oracle/` and
  `out/linux-audio-agent/aa-probe/observations-fixed.json` for public-API oracle and runtime
  evidence. Integrated eight-sample scene run `20261116T082959Z-ed287ee3` shows solid ground
  in `captures/present-00003436-terrain-spawn.png` and exits normally after 39.35 seconds,
  1,954 traced frames and peak RSS 834,532 KiB. The camera differs from the earlier captures;
  this verifies the reproduced defect without granting the wider world-rendering milestone.
  The correction is in `Code/BF2/Platform/Gfx/Metal/D3DXMath.cpp`; regression coverage is in
  `Code/Tests/linux_d3dx/assembly.cpp` and the shared
  `Code/Tests/macos_d3dx_helpers/main.cpp`, with Linux CMake check wiring updated.
- Cold-compilation analysis found 2,810 shader jobs but 792 distinct trimmed inputs. All audited
  full/trimmed outcomes and bytecode match with the current compiler. Generic trimming is unsafe:
  it can remove side-effecting global initializers or hide errors in unused functions. Issue #36
  therefore discusses an optional exact-input equivalence manifest guarded by a compiler
  fingerprint and expected bytecode hash, independently of D3DX acceptance/refusal policy.
  The bounded opt-in implementation and positive/negative regressions pass. The production
  audit contains 2,809 exact-input rows; its compiler fingerprint remains unchanged when that
  manifest is enabled, while the persistent cache identity changes.
- Controlled loading comparison used the same input script and game arguments, including
  `+restart 1`, automatic Operation Clean Sweep sp1/16 hosting on port 16697, a 1280x800 window,
  quality 3 and saved 8-sample AA. Baseline/optimized profiles match within each cache condition.
  Startup plus map loading to Join Game took 120.519 seconds baseline versus 48.715 seconds
  optimized with empty effect/DXVK/Mesa caches: a 59.6% reduction in these individual runs.
  Inclusive effect-request time fell from 106.909 to 36.169 seconds. The optimized run audited
  2,810 jobs, compiled 792 trimmed inputs and reused 2,018, with zero fallback failures and
  1,733,728 bytes retained. Warm runs took 12.114 and 12.045 seconds, with zero synchronous
  compilations and about 0.726 seconds in effect requests. All four runs captured the loaded
  Join Game screen and quit normally. Peak RSS was 891,780/849,312 KiB cold and
  785,456/788,324 KiB warm for baseline/optimized respectively. Evidence and reproduction
  details are in `out/linux-vulkan-load-benchmark/comparison.json`. These are single sequential
  measurements, not statistical gameplay benchmarks. Filesystem cache and continuous thermals
  were not controlled; recorded power settings were AC power, 15 W GPU cap and powersave CPU
  governors. Both cold runs began with saved 3440x1440@99Hz; both warm runs used the game's
  subsequent saved 1280x800@90Hz, with explicit 1280x800 windowed arguments in every run.
  Reuse changes are in `Code/BF2/Platform/Gfx/Effect/NativeEquivalence.{h,cpp}` and
  `Native.{h,cpp}`, `tools/linux/nativefx-equivalence.cpp`,
  `Code/Tests/linux_fx/equivalence.cpp`, their CMake wiring and the explicit syntax gate.
- The owner subsequently confirmed that Operation Clean Sweep loads and plays correctly,
  with the terrain defect resolved. This is human acceptance of the observed local-map
  playtest, not multiplayer reconnect, map-change or full platform certification.
- Progress captures exposed a screenshot-only alpha defect. The same-frame built-in
  `screen000.png` and `screen000.jpg` differ in viewers because PNG retained incidental
  backbuffer alpha; transparent terrain-shaped regions revealed the viewer background.
  The owner's SteamOS capture and side-by-side comparison distinguish this from visible
  game geometry. Both Linux screenshot paths now stage opaque X8R8G8B8 pixels:
  `Code/BF2/Platform/Linux/FrameCapture.cpp` for F12 and the Linux arm of
  `Code/BF2/RendDX9/RendDX9Draw.cpp` for built-in Print Screen. General D3DX surface/texture
  export still retains alpha, and Windows/macOS capture behavior is unchanged. The manual
  `bf2_linux_capture` target (enable `BF2_LINUX_CAPTURE_DEVICE_CHECK`) exercises actual
  capture, resolve and PNG output with one and eight samples. Old code fails its alpha
  check; fixed output preserves every resolved RGB byte and has no alpha channel, while
  direct D3DX export retains the fixture's 2,304 nonopaque pixels. Logs are in
  `out/linux-audio-agent/capture-{before-,}device.log`. Full-client run
  `20261116T095644Z-66abcb55` also produced opaque RGB PNGs through both paths, then quit
  normally after 46.67 seconds. This used a separate state directory and port 16698,
  without controlling the owner's process or replacing its executable during testing.
- The native console's background was fully transparent because
  `Code/BF2/Platform/Gfx/Metal/Effect.cpp` converted its RGBA TextureFactor expression from
  only the first component. Original D3DX9_25 state-manager callbacks publish 0xcc000000
  for `(0,0,0,0.8)`. The native adapter now packs proven TextureFactor/FogColor float4 forms
  using the original clamping and binary32 rounding behavior. Scalar conversions and
  compiler/cache identities are unchanged. Eight original color cases and twelve byte-boundary
  cases pass through the real native effect callback path in
  `Code/Tests/linux_fx/packed_colors.cpp`; the previous adapter fails. A broader 765-case
  original-runtime boundary audit establishes the single-precision arithmetic, as discussed
  in issue #36. The corrected scalar control is exactly representable in the existing
  float preshader; arbitrary large-integer preshader fidelity is outside this change.
  The isolated retail run on port 16698 shows a pale translucent, full-width 244-pixel panel
  with black text in `out/linux-wine-console-comparison/retail-console-{empty,query}.png`.
  Native run `20261116T095842Z-69521258` matches that panel appearance and displays the
  `renderer.drawfps` query and its result in `captures/present-00003628-console-query.png`.
  The scene poses differ, so this is a console appearance comparison, not a pixel-matched
  world-rendering baseline. Both screenshot paths produced RGB PNGs without alpha in this
  run, which exited normally after 46.48 seconds and 2,326 frames (peak RSS 801,636 KiB).
  Native font, dimensions and recovered console drawing order are unchanged. The tested
  executable SHA256 is `41bd9231d53cc44934d2689d65e93603a2e9906bc091fc8393e186f1eba65701`;
  it is retained at `out/linux-vulkan-review-client/bf2_client` as the pre-quit-fix baseline.
- The owner's earlier run `20261116T083217Z-b702db9c` lasted 4,612.10 seconds (about
  77 minutes), returned to the menu, then exited with SIGSEGV during renderer shutdown.
  The saved stack reaches `rendererReleaseD3DDevice()`; core storage is inaccessible,
  so the precise object state in that incident is unavailable. The old executable and
  companion libraries are retained under
  `out/linux-vulkan-owner-session-20261116T083217Z-b702db9c/`, and the saved stack and
  analysis are in `out/linux-host-agent/owner773118-{coredump-info.txt,shutdown.md}`.
  Earlier short runs used script/window quit, which bypasses renderer teardown; their
  exit status does not validate the game's QUIT path.
- Adopted the saved-depth-surface release from [PR #37](https://github.com/kr4uzi/OpenBF2/pull/37),
  merge `1c1bc384511e5b5d8409201e308ec13202b88afa`, following the intentional fix discussed
  in [issue #35](https://github.com/kr4uzi/OpenBF2/issues/35#issuecomment-6899384399).
  `Code/BF2/RendDX9/BF2Render.cpp` now releases the reference returned by
  `GetDepthStencilSurface` after restoring it. This is `[CHOSEN, not recovered]`, not a new
  reconstruction claim; ordering, repeated AA queries, interfaces and layouts are unchanged.
  The forwarding reference model and final device-release diagnostics remain unchanged.
  The shared omission retained one surface reference per multisampled frame; on DXVK the
  externally referenced surface keeps the device alive without adding a new device reference
  on every frame. `Code/Tests/linux_d3dx/device.cpp` now checks the public-device consequence:
  three balanced eight-sample brackets keep device references at 1 and final Release returns 0;
  the omission control leaves 2 device references and final Release returns 1, then cleans up
  its retained surfaces. The manual GPU suite passed (`out/linux-audio-agent/overlay-device.log`).
  The native client also compiled and strictly linked successfully.
- Actual game-menu QUIT checks used Operation Clean Sweep, 800x600 windowed, saved eight-sample
  antialiasing, isolated profiles, copied content and port 16698. Both scripts drive the existing
  input interpreter and click YES; neither uses the script `quit` command. The baseline
  `20261116T103428Z-e5ec390b` reproduced SIGSEGV during renderer shutdown after 75.87 seconds.
  With the fix, quitting in-map (`20261116T103556Z-732999f4`, 32.21 seconds, 1,331 frames) and
  disconnecting to the main menu before quitting (`20261116T103711Z-2b9f49be`, 41.34 seconds,
  2,506 frames) both exited 0 and completed renderer deinitialization without timeout or
  interruption. Scripts are `out/linux-quit-{in-map,after-map}.input`; screenshots and logs
  are in those run directories. Durations are not a performance comparison: caches differed.
  Two preceding Xvfb attempts ended with display I/O errors before gameplay, so acceptance
  uses the real desktop's non-focusable scripted windows. The owner's concurrent port-16697
  playtest was not controlled or stopped; that pre-fix run (`20261116T102919Z-d048449a`)
  independently ended at the same renderer-shutdown point with SIGSEGV after 184.08 seconds.
  The validated executable SHA256 is
  `ae7afd810753c7ad628de3659b341640ae936d53c8640f22c2c63441c5f8dc6f`, installed atomically at
  `out/linux-vulkan-client/bin_linux/bf2_client` for subsequent launches; the earlier binary
  remains available for comparison. These checks establish both short quit paths;
  the 77-minute session has not been repeated with the fix, and Windows/macOS were not rebuilt.
- Windows generator and test-fixture output compared with the pre-change generator:
  identical. The checked-in Windows generated files were already stale relative to the
  current subset headers; they were not regenerated in this change.
- Manual graphics run `20261115T190846Z-8a55db9f`: exit 0, three saved BMPs, owner reported
  "All looked correct" for window, colors, resize/fullscreen, focus and screenshots.
  The cold-cache light capture contains 4,168 frames over 42.64 seconds on RADV VANGOGH
  26.2.5. Median interval: 10.00 ms; p99: 11.25 ms. Its one interval over 50 ms is
  773.62 ms at startup (frame 2), predominantly in the preceding Present call. Peak process
  RSS: 126,772 KiB. This is presentation-test timing, not a game performance result;
  power settings and GPU timing remain unknown, and no warm-cache comparison was made.
- Not run: MSVC x86 full syntax/all-TU gates, MSVC x64 strict link, macOS build,
  full Linux ARM64 build/runtime, complete world-scene review, multiplayer/reconnect/map-change
  tests or longer stability tests after the quit fix.

Changes cover the D3D9 subset and generated DXVK forwarding boundary; portable D3DX/effects;
SDL window/input and shared scripts/captures; ELF plugin isolation; audio/video, Python and AI
assembly; physical asset paths and protected profiles; Linux selection of existing numeric
adapters and direct binary128 conversion; and opt-in build, dependency, launcher and test tools.
No recovery frontier has been retired; the dependency loader and unsupported resource
dispatch still fail loudly. Human acceptance covers the standalone window and observed
Operation Clean Sweep local gameplay; broader gameplay and platform validation remain open.

Relevant retained boundaries include `macos-d3dx-assembler`, `macos-dds-mip-skip`,
`macos-mesh-point-reps` and `macos-mesh-semantics` in the shared helper sources, the unsupported
Bink RDFT/Bink2 decoder paths, and native editor process-tool stops. The new
`linux-quality-calibration` marker keeps the conservative CPU-family multiplier explicit;
automatic quality is not a calibrated Linux hardware benchmark. Linux system mixer controls
remain unavailable. Voice codec quality changes are tested; PCM rate/frame-size changes require
reopening devices. Physical controller behavior, Gaming Mode and audible menu acceptance remain
unvalidated.

| Milestone | Status and acceptance boundary |
| --- | --- |
| Window and presentation | Standalone boundary visually accepted by owner; full BF2 client links and creates its window. |
| Menu | Owner reported good overall appearance and confirmed the corrected cursor. Complete visual/audio review remains pending. |
| World rendering | Shader loading and terrain defects fixed; owner confirmed Operation Clean Sweep looks correct and plays. Console appearance compared with retail; screenshot-only alpha defect fixed. |
| Playable client | Owner confirmed local-map gameplay. The shutdown crash reproduced before upstream PR #37's fix; both in-map and return-to-menu QUIT paths passed afterward. Multiplayer, reconnect, map changes and longer stability remain unvalidated. |

Continue the existing shared input-script interpreter and `mark NAME` / `goto NAME` workflow.
Do not introduce another gameplay harness. Camera-before-chat recording, authoritative pose
correlation, convergence checks and human-approved first replays remain planned work. No
gameplay visual baselines have been created or accepted by this checkpoint.
