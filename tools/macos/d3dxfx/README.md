# D3DX9_25 shader experiment

**Developer oracle, not used by the client.** These optional Wine/D3DX reference
tools are outside the client build. The client compiles effects natively; see
[building](../../../docs/building.md#macos).
`patches/` holds shared native-source patch data; `patch-mojo.py` delegates to the
native utility without invoking Wine or D3DX. Exporter/runtime checks in the root CMake project
require `-DBF2_FX_DEVELOPER_ORACLE=ON` and explicit build targets.

[CHOSEN, not recovered] Standalone owner-requested macOS port tooling. It never links
into an engine image.

The s1c coverage work measured this
result: 219/269 D3DX attempts accepted (including the two original controls), all
219 effects parsed, 982/982 shaders translated and Metal libraries compiled.
The 50 remaining failures are genuine lower-profile attempts followed by engine
fallback; they are retained. `STM1_4.fx` is an inactive permutation fragment and
is excluded instead of inventing its missing generated inputs. No runtime effect
or draw acceptance is implied. Dynamic state expression payloads are retained,
but their evaluator remains a diagnostic-and-abort frontier.

From the repository root, without downloads or installation:

```sh
tools/macos/d3dxfx/run-all.sh --controls --coverage-controls --metal --output out/d3dxfx-s1c
python3 Code/Tests/macos_d3dxfx/check-coverage.py
```

`run-all.sh` builds and patches the tools, derives engine jobs from source and
content, and invokes Wine once for the whole batch. `--metal` then runs the Metal
gate and writes `metal-summary.json` with failure classes. The current default
is 269 attempts with `--controls`: 39 legacy candidates, 29 direct Ra effects and nine generated
permutations at six supported profile combinations, plus two controls. Lower
profile refusal is expected: use `effect-groups.json` for the 79 logical groups;
compiler acceptance does not select the runtime's first validated technique.
`--profiles sm2` reduces this to 79 attempts; `--all-permutations` generates every
listed permutation and each modifier (8,195 attempts with controls). The default
samples the first three base permutations per metadata file. `--inputs empty`
restores the 120-job s1 baseline with controls. Source inputs default to the sibling s0 worktree's
`out/fx-sample/src/`; override with `--source DIR`. All generated sources, copied
DLLs, compiled binaries, MSL, hashes and diagnostics stay in this worktree's `out/`.

The Wine loader gets the app's read-only `Contents/Frameworks` through its process
environment. The prefix is created at `out/wineprefix-s1-d3dx-wine/`; an existing
unmarked prefix is refused. The game's prefix is never used. The compiler loads
only `d3dx9_25.dll` beside its own executable and resolves the compiler entry point
dynamically. No D3DX import library or Direct3D device is needed. Wine's HOME,
temporary files and cache are redirected beneath the selected output directory.

Useful partial runs:

```sh
sh tools/macos/d3dxfx/build.sh
tools/macos/d3dxfx/run-all.sh --skip-build --prepare-only --controls
tools/macos/d3dxfx/run-all.sh --skip-build --stage2-only
python3 tools/macos/d3dxfx/check-controls.py
tools/macos/d3dxfx/metal-compile out/d3dxfx-controls/msl out/d3dxfx-controls/results.csv
```

`build.sh` uses MinGW i686 for `fxcompile.exe`, copies the already available local
MojoShader source before its CMake build (its version-header generator writes in
the source directory), and uses Apple clang / Foundation for `effectparse` and
Foundation / Metal for `metal-compile`. Builds are under
`out/build/s1c-fx-coverage/tools/`, with Unix Makefiles for MojoShader.
`MOJOSHADER_SOURCE` and `MINGW_CC` may select already installed local inputs.
The local zlib patches under `patches/` apply at every build, idempotently with
zero fuzz; drift fails the build. `patches.json` records patch/source hashes.
`--coverage-controls` adds three separate regression effects (eight libraries),
with their own `regression` summary group. The additional native control check
compiles three more SM1 libraries, tests reference updates/clone ownership,
rejects malformed references/bytecode, verifies the expression frontier, and
checks all 751 original passing sources remain identical and Metal-ok.

Direct Win32 usage (paths are Windows paths under Wine):

```text
fxcompile SOURCE OUT.fxo ERRORS.log INCLUDE_ROOT [OPTIONS | NAME=VALUE | @DEFINES_FILE ...]
fxcompile --batch JOBS.tsv
```

TSV rows contain source, output, error-log and include-root paths, then one optional
define or `@DEFINES_FILE` per additional tab-separated field. Paths with spaces need
no TSV quoting. Tabs/newlines are forbidden in fields. Define files contain one
`NAME=VALUE` per line; blank lines and lines beginning `#` are skipped. A bare name
means `NAME=1`. Paths and source contents use the local ANSI/ASCII corpus, not a
general Unicode Windows interface. Paths must fit Win32 `MAX_PATH`.

Options are `--flags=NUMBER`, `--include-mode=baseline|legacy|ra` and
`--virtual-root=WINDOWS_PATH`. Engine jobs ignore include type/parent as both
engine callbacks do; the flat corpus maps `shaders/`, `mods/bf2/shaders/` and
`mods/xpack/shaders/` onto one extracted archive. Other mounted paths fail visibly.
Ra's two attested virtual `RaDefines.fx` paths read the generated virtual root;
legacy reads the physical file. Baseline retains the old parent-relative search.
Wine provides Windows case-insensitive lookup. This is a single-archive mounted
filesystem model, with no claim to general mod layering or file-change watches.
Both parse and `CompileEffect` diagnostic buffers are retained, including warnings.
The engine snapshot uses `0x100` (no preshader); baseline/controls use zero. Both
compiler calls receive the job's flags. Each job writes `.fxo.status` (`ok`/`fail` and both HRESULTs), and a
failed job removes any previous binary. Batch exit 1 means at least one job failed;
remaining jobs are still attempted. Exit 2 denotes setup/usage/I/O failure.

Native parser usage:

```text
effectparse EFFECT.fxo OUTPUT_DIR
effectparse --shader ENTRY SHADER.bin OUTPUT_DIR
```

It reuses s0's BYTECODE capture, effect inventory and Metal translation approach.
It retains per-object bytecode, MSL, shader entries and register metadata; pass
state object indices connect shared shaders to passes. It translates every compiled
shader object, including objects selected through preshaders, but does not evaluate
preshaders or execute effects. `no_shaders` is distinct from a translation success.
`--shader` is a standalone diagnostic that bypasses effect parsing. The parser is
not hardened for untrusted FX binaries; subprocess isolation retains upstream traps.

`results.csv` uses separate `effect` and appended `metal` stage rows. `ok`, `fail`,
`not_run`, `no_shaders` and `unavailable` distinguish outcomes. The five downstream
controls use their own CSV and `compile=provided_s0`, never a Microsoft compiler
success. `summary.json` separates legacy, direct Ra, Ra permutations and controls
(or empty defines for baseline). `engine-inputs.json` records the configuration,
source hashes, macro namespaces, profiles, include graph and each input's role.
Pure includes, fragments, metadata and permutation templates are excluded from
standalone effect rates. Root selection from the legacy include graph is labelled
`[INFERRED]`; the caller may still request another content effect at runtime.
Shader failures are measured outcomes: the collector exits 0 after a complete
collection, 3 if compiler jobs were unattempted, 2 on setup failure. Inspect counts.
`--stage2-only` rewrites the CSV; `--metal` appends fresh Metal rows afterwards.
The classifier uses the latest row per shader when Metal has been repeated.

Regression without Wine/Metal, using the retained coordinator binaries:

```sh
python3 Code/Tests/macos_d3dxfx/check.py out/d3dxfx/results.csv
```

This replays the native translator and checks the 18 reported source defects
and that all 48 previously passing sources remain identical. It requires the
original s1 results; it is not a Metal acceptance test.

`metal-compile` recursively compiles every `.metal` with `newLibraryWithSource`,
checks the recorded entry point, writes adjacent `.metal.log` diagnostics and appends
one CSV row per file. It returns 0 only if all libraries/entries succeed, 1 on a
compile failure or no inputs, 2 on setup failure, 3 when no Metal device is available.
It tests libraries, not render pipelines, shader linkage, draws or visual output.

## CPU effect cache

The separate `bf2_fx_runtime` library holds the versioned cache, include closure/key,
parameter store, expression evaluator and G4 callback contract. Build its offline exporter with:

```sh
sh tools/macos/d3dxfx/build-runtime-tools.sh
python3 tools/macos/d3dxfx/build-cache.py --help
```

`build-cache.py --manifest ... --source ...` prewarms detailed s1b/s1c job inputs;
`--requests <cache>/requests.txt` compiles only native runtime misses from their
captured source/include snapshots. It uses a fresh worktree-local Wine prefix,
refuses compiler/translator drift and publishes no entry on compiler or parser
failure. The cache builder performs no game launch or Metal rendering.

The engine's complete request set is derived statically by `enumerate_requests.py`
(engine source plus the read-only Shaders archive, per video-quality configuration and
the Metal device caps) and built by `prebuild.py`, which compiles only the attempts the
engine actually reaches and runs the Metal gate. `validate_requests.py` checks the
grammar against s1b/s1c manifests and s2 loader requests. Grammar, coverage, size,
build time and policy: [c-fx-permutations](../../../docs/macos-c-fx-permutations.md).
