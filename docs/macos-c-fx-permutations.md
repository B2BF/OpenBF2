# Effect request set: static enumeration, prebuild and coverage

Current policy: the client has no Wine route; r2-no-wine removed the Wine cache route from
the client and build. Wine/cache commands and results below are historical developer
oracle work, not used by the client. Use [native compilation](building.md#macos) today.

[CHOSEN, not recovered] Owner-requested macOS port tooling ([macos-port.md](macos-port.md)).
No engine source, recovered body or MSVC x86 path changes. The tools read the recovered
source in this tree and the read-only game content; all generated content (requests,
roots, cache, logs) stays under ignored `out/`. No game, client, server or port was run.

The macOS effect path is cache-based: a miss records a
request and fails. This package answers which requests the engine forms for the shipped
content, prebuilds them, and measures how many survive D3DX, MojoShader and Metal.

## Tools

| File | Purpose |
| --- | --- |
| `tools/macos/d3dxfx/enumerate_requests.py` | Static walk: engine tables and call sites from source, Metal caps from `Platform/Gfx/Metal/Device.cpp`, Shaders archive read in place. Writes `requests.json` (unique requests), `groups.json` (per config and effect: the ordered profile attempts), `jobs.json` (build-cache manifest), `summary.json`, and the exact root and `RaDefines.fx` bytes. |
| `tools/macos/d3dxfx/prebuild.py` | Builds the engine-reachable prefix of each attempt chain, round by round (round r = attempt r of every unresolved group) through `build-cache.py`. Writes `state.json` and `summary.json` with refusal classes. |
| `tools/macos/d3dxfx/validate_requests.py` | Recomputes keys with the production capture and compares against s2 loader requests and the s1b/s1c manifests. |
| `tools/macos/d3dxfx/build-cache.py` | A job may carry its own `virtual_root` (one `RaDefines.fx` per configuration). `--parallel N` runs round-robin `fxcompile` chunks in the one fresh prefix (after an empty batch initializes it) and captures/publishes with N threads. Results list every manifest job that formed a key (`aliases`). The per-run `fxcompile.exe` copy is marked large-address-aware, as `BF2.exe` 1.50 is (PE characteristics 0x12E); the pinned artifact is unchanged. |
| `tools/macos/d3dxfx/dev-route-limits.json` | The 8 SkinnedMesh permutations the dev route cannot compile (gap 3), for c-native-fx. |
| `tools/macos/d3dxfx/patches/0005-metal-array-uniform-define.patch` | MojoShader Metal fix: no `;` after `#define cN uniforms.uniforms_float4[i]` inside relatively addressed arrays. Changes the translator identity. |

```sh
sh tools/macos/d3dxfx/build-runtime-tools.sh
mkdir -p out/c-fx/corpus && unzip -oq ".../mods/bf2/Shaders_client.zip" -d out/c-fx/corpus   # include corpus for capture
python3 tools/macos/d3dxfx/enumerate_requests.py --classes client,conditional,unrecovered --output out/c-fx/requests
python3 tools/macos/d3dxfx/prebuild.py --requests out/c-fx/requests --cache out/c-fx/cache-all \
  --work out/c-fx/prebuild-all --parallel 4 --metal          # every config; --config ID to restrict
# (the measured run used out/c-fx/requests-v2; --max-rounds 4 stops before gap 3's chains)
python3 tools/macos/d3dxfx/enumerate_requests.py --caps s1b-sm2 --config L3S1F3T3 --output out/c-fx/requests-s1b-sm2
python3 tools/macos/d3dxfx/validate_requests.py --requests out/c-fx/requests-s1b-sm2 --config L3S1F3T3 \
  --corpus out/c-fx/corpus --s2 out/fx-cache-s2-v1 --s1b out/d3dxfx-s1c/jobs.json
```

The enumerator itself only reads the zips in place. The extracted corpus exists because
`bf2_fx_cache_tool capture` resolves includes from a flat directory.

## Request-key grammar

A cache key (s2) is SHA-256 over: root bytes, the canonical macro set, compiler flags,
DLL hash, translator identity, and the literal-include closure (spelling, type, parent,
exact callback bytes). The engine forms exactly four request kinds. Everything below is
read from the recovered source; line numbers are at commit 9844a04.

```text
request      := ra_metadata | ra_direct | ra_generated | legacy
config       := lighting(1..3) shadows(0|1) filter(1..3) terrain(1..3)   + device caps, mod
ra_attempts  := [ (VSMODEL, PSMODEL, _FORCE_1_4_SHADERS_, _FORCE_1_3_SHADERS_=0, LOWPSMODEL=ps_1_4, PSVERSION) ]
                in engine order, stop at the first created effect with a valid technique
ra_direct    := root = file bytes of shaders/RaShader<name>.fx, macros = ra_attempt, flags 0x100,
                includes via Ra include (RaDefines.fx -> virtual text(lighting, filter, caps))
ra_generated := root = MetaFx wrapper(family, permutation), same macros/flags/includes as ra_direct
ra_metadata  := root = .mfx bytes + NUL, macros = {_FORCE_1_4_SHADERS_}, flags 0x100, no include
legacy       := root = file bytes of shaders/<Name>.fx, macros = ShaderManager vector(filter, terrain, caps),
                flags 0x100, includes via ShaderManager include (physical files only)
```

### Who asks, and when

| Kind | Requester | Code |
| --- | --- | --- |
| ra_direct | `raLoadAllShaders(table, 3)` on every video-options apply: `findFiles("shaders/")`, keep names containing `rashader` and `.fx`, not `common`, not the three masters; name = stem after `rashader`. `findOrCreateShader` builds at once because `GSDeferredShaderPermutation` defaults false and base `+0x102` is never written. Also `"default"`. | `Ra/EffectManager.cpp:1851`, `:1865`; `Ra/RaShader.cpp:1537`, `:1589`; `BF2Render.cpp:4264` |
| ra_metadata | `raLoadAllShaders` flag 1: `loadMetaFx` per `.mfx`, `MetaFx::parse` -> `generate()` calls `D3DXCreateEffectCompiler` for reflection only (never `CompileEffect`). | `Ra/EffectManager.cpp:657`; `Ra/RaShaderPermutator.cpp:752`, `:771` |
| ra_generated | First template load: `bundledMeshInitTechniques`, `skinnedMeshInitTechniques`, `staticMeshInitTechniques` -> `staticMeshBuildTechniqueMasks`: every generated permutation whose bits survive the quality mask is registered with `getShader(prefix + name, true)` and built immediately. **Content-independent**: a level's materials only choose among registered shaders (a missing combination falls back to `Default`, `BundledMeshTemplate.cpp:615`). | `Geom/StaticMeshTemplate.cpp:252`, `:396`; `Geom/BundledMeshTemplate.cpp:607`; `Geom/SkinnedMesh.cpp:308` |
| legacy | Fixed `shaderManagerGetEffect` call sites (table below); one compile per lowercased, mod-stripped key. | `ShaderManager.cpp:742`; `Effect.cpp:1061` |

### Ra profile attempts (`Ra/EffectManager.cpp:1317`, loop at `:1511`)

The macro array is `VSMODEL, PSMODEL, _FORCE_1_4_SHADERS_, _FORCE_1_3_SHADERS_=0,
LOWPSMODEL=ps_1_4, PSVERSION`. Outer loop `vs_1_1, vs_2_0`; inner `ps_1_3, ps_1_4,
ps_2_a, ps_2_b, ps_2_0` (`PSVERSION` 13/14/21/22/20), skipping rows below
`kMinPsVersion` (`:1148`), `ps_2_a` on ATI without arbitrary swizzle and `ps_2_b` on
nvidia. `_FORCE_1_4_SHADERS_` is 1 only below ps 2.0 or with the debug/force bytes.
The Metal device reports VS/PS 3.0 and vendor 0 (`Device.cpp:25`, `GetAdapterIdentifier`
zero-fills), so all **ten** attempts are eligible, in this order:
`vs_1_1/ps_1_3, vs_1_1/ps_1_4, vs_1_1/ps_2_a, vs_1_1/ps_2_b, vs_1_1/ps_2_0, vs_2_0/...`.
The engine stops at the first attempt whose `D3DXCreateEffect` yields an effect with a
technique from `FindNextValidTechnique`. Flags: `g_noPreshader` is 1, so 0x100;
create flags add 0x800 when `m_byte62`. A refused attempt is a compile failure the
engine already tolerates (`continue`), so on macOS a refused key simply keeps missing
and the engine falls through to the next attempt, as on Windows.

### Virtual `RaDefines.fx` (`Ra/EffectManager.cpp:385`, `:1702`)

The Ra include returns `shaderDefines + debugDefines` for `shaders/RaDefines.fx` and
`mods/xpack/shaders/RaDefines.fx`, byte for byte as `rebuildShaderDefines` prints
them, **including trailing spaces** (`"#define RAPATH %i \n"`) and without them on the
two `MAX_ANISOTROPY` lines. Inputs: vendor, SM30/SM20 from caps, `RAPATH` = Renderer
`+0x6CC`, alpha-to-coverage (multisample active and probe), A2M codes (`0x%X`),
texture-filter quality `quality[7]` with the caps' anisotropy limits. For the Metal
device (no multisample, min-anisotropy only, MaxAnisotropy 16) and quality 3:

```text
#define NVIDIA 0
#define ATI 0
#define SM30 1
#define SM20 1
#define RAPATH 0 ␠
#define HASALPHA2MASK 0 ␠
#define Alpha2Mask PointSize ␠
#define A2MEnable 0x0 ␠
#define A2MDisable 0x0 ␠
#define FILTER_STM_DIFF_MIN ANISOTROPIC ␠
#define FILTER_STM_DIFF_MAG LINEAR ␠
#define FILTER_STM_DIFF_MAX_ANISOTROPY 2
#define FILTER_BM_DIFF_MIN ANISOTROPIC ␠
#define FILTER_BM_DIFF_MAG LINEAR ␠
#define FILTER_BM_DIFF_MAX_ANISOTROPY 4
#define FILTER_STM_NORM_MIP LINEAR ␠
#define FILTER_BM_NORM_MIP LINEAR ␠
```

`RAPATH` comes from lighting quality: `quality[2]` 1/2/3 sets `+0x6CC` to 3/1/0, and
lighting 1 also forces dynamic shadows off (`VideoOptions.cpp:418`). `RaShaderBM.fx`,
`RaShaderSM.fx` and `RaShaderSTM.fx` test `RAPATH`. Distinct texts: 3 lighting x 3
filter = **9**.

### Generated permutations (`Ra/RaShaderPermutator.cpp`)

- Literal names: engine regex `^const\s+(bool|int|dword)\s+(\w+)\s+:\s+LITERAL.*` on the raw
  `.mfx` lines (`:360`), not on D3DX reflection.
- `GenerateStructs`, `<struct>Base`, per-literal annotations named `<struct>`,
  `GenerateShaderPermutations`, `GenerateShaderModifiers`, `DeprecationList`: D3DX string
  arrays (`readStringArrays`, `readDeprecationList` `:929`). Names generated per base
  permutation p: `p`, then `p|m` for each modifier m (`:771`). BM 32 x 33 = 1,056, SM
  12 x 7 = 84, STM 19 x 10 = 190.
- Wrapper bytes (`generateShader`, `:1006`), `\n` line ends:
  `//`, `// AutoGenerated shader file <m_name><clean>`, `//`, empty, `// Shader setting `,
  `#define <LITERAL> 0|1` per literal, `static const dword <literal> = 0|1;`, then per
  struct with members `\nstring <S>[] = {\n\t"a",\n\t"b"\n};` (base members, then each
  enabled literal's annotation, first occurrence wins), `#define NUM_LIGHTS 1 `,
  `#define NUM_TEXSETS n`, `#define Tex<X>Ind i` for members `T<X>2D`, and
  `\n#include "shaders/<RaShaderXX>.fx"`. `m_name` is the mod-prefixed stem,
  `mods/bf2/shaders/RaShaderBM`; `clean` drops `_` and `|`.
- Registration (`Geom/StaticMeshTemplate.cpp:252`): bits = OR of each cleaned token's
  index in the family table (BM `kBits`, SM `kSkinnedMeshShaderBitNames`, STM
  `kStaticMeshShaderBitNames`); duplicate bit sets keep the first name (`:1163`). Mask
  vector `[~0, ~row0, ~(row0|row1), ...]`, padded to 4; index = `+0x6CC`. Without
  dynamic shadows the mask also drops `hasshadow` (BM/SM; STM's table names it `shadow`,
  so STM is unaffected). A permutation is built when `(mask & bits) == bits`.
- Effect name: `shaders/rashader` + lowercase(prefix + clean) + `.fx`, read from the
  in-memory file `mods/bf2/shaders/RaShader<XX><clean>.fx` (`readEffectFile`, `:1004`).

| Registered permutations | lighting 3 (`RAPATH 0`) | lighting 2 (`RAPATH 1`) | lighting 1 (`RAPATH 3`) |
| --- | ---: | ---: | ---: |
| BM, shadows on / off | 1,056 / 576 | 528 / 288 | - / 32 |
| SM, shadows on / off | 84 / 36 | 28 / 12 | - / 3 |
| STM (shadows ignored) | 190 | 110 | 20 |

### Legacy effects (`ShaderManager.cpp:197`, `:742`; `Effect.cpp:1061`)

Macros: the 120-entry `ShaderManagerDefines.inc` table with nine substitutions, then
`A2MEnable`/`A2MDisable` (`%X`), `_FORCE_1_4_SHADERS_`, `_FORCE_1_3_SHADERS_`, the
filter block (`quality[7]`), the three mip filters and the terrain pair (`quality[0]`:
1 nothing, 2 mid, 3 high). 131 to 135 macros, no duplicates. Flags 0x100 (non-nvidia).
Root = file bytes; `mods/xpack/...` names resolve to the mounted Shaders archive through
`FileManager::stripModPath`. Includes use the physical files (the archive's dummy
`RaDefines.fx` for `SwiffMenu` and friends). Distinct macro vectors: 3 x 3 = 9.

| Class | Effects |
| --- | --- |
| client (19) | BundledMesh, Decals, Font, FSQuadDrawer, LoadingScreen, MeshParticleMesh, Nametag, NonScreenAlignedParticles, Particles, PortedFontShader, PortedMenuShader, RoadCompiled, SkinnedMesh, SkyDome, StaticMesh, SwiffMenu, TerrainShader, Trail, Undergrowth |
| conditional (2) | LightManager_r3x0 (`GSRenderPath` 2; default 3, `BF2Render.cpp:1334`), DebugLineGraph (debug graph) |
| unrecovered (4) | Rain, Lightning, PostProduction_nv3x, DebugD3DXMeshShapeShader: present as strings in the shipped `RendDX9.dll`, no recovered caller |
| editor (5, not built) | SplineShader, Road, LightGeom, QuadGeom, TerrainEditorShader |

Nine more root-level effects in the archive are never named, by the recovered code or
the shipped DLL: DebugCircleShader, DebugSphereShader, PointSpriteParticles,
PostProduction_r3x0, SimpleAlphaBlendShader, TerrainUGShader, TreeBillboard, TreeMesh,
TreeMeshBillboardGenerator. s1b's inventory compiled them as candidates.

### Settings that do not change the request set

Dynamic lighting `quality[3]` only adds `Leafpointlight*` registrations that the
directory scan has already built; geometry, effects, texture quality and view distance
do not reach any compiler input. Debug defines, `GSQuickShaderPermutation`, the force
bytes and partial precision are off by default and are not enumerated.

## Enumeration result

45 configurations (lighting x shadows x filter x terrain, lighting 1 without shadows):

| | Per config (max, L3S1) | Unique over all 45 |
| --- | ---: | ---: |
| Effect groups | 1,387 (3 metadata, 29 direct, 1,330 generated, 25 legacy) | 6,634 (rank-0 requests) |
| All attempts (upper bound) | 13,618 | 64,288 |
| Distinct root sources | | 1,387 |
| Distinct `RaDefines.fx` | | 9 |

`ra_direct` 2,530, `ra_generated` 61,530, `ra_metadata` 3, `legacy` 225 unique
requests. Unique means distinct key inputs: a root whose literal-include closure never
reaches `RaDefines.fx` (only `RaShaderDefault.fx`) shares one key across the nine
`RaDefines.fx` texts. Content check: the only mounted shader source is `Shaders_client.zip`
(109 files, plaintext, no obfuscation magic); no other mod or level archive carries
`.fx`/`.dfx`/`.mfx`; `mods/bf2/shaders/` is empty. One fixed Ra name does not resolve:
`Water` (expansion pack only; `getShader` returns the default shader, no compile).

## Grammar validation

`validate_requests.py` with the enumerator run as `--caps s1b-sm2` (s1b's SM2 snapshot):

| Against | Result |
| --- | --- |
| s2 loader requests (`out/fx-cache-s2*/requests/*.request`, 48) | All 28 legacy requests (BundledMesh, Font, SwiffMenu; 12 of them stored with explicit flags 0) reproduce **byte-identical keys** from the enumerated root, macros and include closure. All 9 Ra requests (`RaShaderWaterDistant2D`) match root and macros and differ only in `RaDefines.fx` bytes. 11 are synthetic controls with no engine root. |
| s1c `jobs.json` (270 attempts) | 19 legacy, 174 direct Ra and 54 generated attempts: macros and flags identical. Generated wrappers are semantically identical (same macros, literals, arrays, include) but not byte-identical. `RaDefines.fx` differs for all Ra jobs. The other 20 legacy inputs are outside the client class (2 conditional, 4 unrecovered, 5 editor, 9 never named). |
| D3DX reflection of the three `.mfx` | Element counts of every string array and the literal sets match `fxcompile`'s reflection log. |

So the s1b/s2 Ra prewarm never produced a key the engine will look up: s1b wrote
`#define K V\n` without trailing spaces and its own wrapper format. The enumerator's
bytes follow `rebuildShaderDefines` and `generateShader`; with the Metal caps the
legacy values also change (`SM30 1`, magnification `LINEAR`).

## Build and coverage

### One configuration: L3S1F3T3 (lighting, shadows, filter, terrain all high)

`prebuild.py --config L3S1F3T3`, Intel i7-8569U (4 cores), Wine from the BF2 app bundle,
BF2's `d3dx9_25.dll`. Rounds 0 to 2 ran with one `fxcompile` process, later rounds with
four. Compiler results do not depend on the translator, so this table stands after patch
0005 (see Metal below).

| Round (attempt) | Jobs | Published | Wall time |
| --- | ---: | ---: | ---: |
| 0 `vs_1_1/ps_1_3` | 1,387 | 82 | 242 s |
| 1 `vs_1_1/ps_1_4` | 1,302 | 166 | 266 s |
| 2 `vs_1_1/ps_2_a` | 1,136 | 1,116 | 1,392 s |
| 3 `vs_1_1/ps_2_b` | 20 | 12 | 133 s |
| 4 to 9 (eight SM permutations, every remaining attempt) | 8 each | 0 | 1,580 s |

Of 1,387 groups, 1,376 resolve: 82 at attempt 0, 166 at 1, 1,116 at 2, 12 at 3. The
engine forms **3,893 requests** for this configuration (each group's attempts up to its
first success), 28% of the 13,618-attempt upper bound. On the Metal caps the engine
therefore settles on `vs_1_1` with `ps_2_a` for most Ra effects, as it would on any SM3
Windows card: the profile loop never reaches `vs_2_0` for a group that resolves earlier.

Refusals by D3DX (2,517), all of which the engine survives by moving to its next attempt
(`continue` in `compileEffect`):

| Class | Count | Engine behavior |
| --- | ---: | --- |
| X4502 texcoord/register reference beyond the model | 796 | next attempt |
| X4532 expression not mappable (incl. `tex2Dproj` on ps_1_3) | 429 | next attempt |
| X4512 texcoord `.w` in a general expression (ps_1_x) | 356 | next attempt |
| X4510 sampler count above the model | 296 | next attempt |
| X4500 program too complex for ps_1_4 | 257 | next attempt |
| X4520 texcoord read plus sampling (ps_1_x) | 190 | next attempt |
| X4516 texcoord shared by samplers (ps_1_x) | 94 | next attempt |
| X4507 constant register limit / X4523 dependent read | 14 / 14 | next attempt |
| X4521 out of temporary registers | 8 | next attempt |
| E_OUTOFMEMORY in `CompileEffect` | 60 | next attempt; see gap 3 |
| "There were no techniques" (`.mfx`) | 3 | not an engine compile; see gap 1 |

By kind: all 25 legacy effects publish at their only attempt; direct Ra effects resolve
21 at attempt 0, 4 at 1 and 4 at 2 (the 3D water effects); generated permutations 36 /
162 / 1,112 / 12 at attempts 0 to 3, 8 never. Every compiler success also parsed,
translated and published (0 publication failures).

Metal (`metal-compile`, `newLibraryWithSource` on the Intel Iris Plus Graphics 655): the
1,376 entries hold 3,227 MSL libraries. Before patch 0005, **2,763 compiled and 464
failed**, all with one defect: MojoShader's Metal profile emitted
`#define c1 uniforms.uniforms_float4[1];` for constants inside a relatively addressed
array (BundledMesh bone palettes), and the semicolon broke `r0.w = r0.y + -c1.w;`.
The patch removes it; a rebuilt failing entry then compiled 2/2. Because the patch
changes the translator identity, the full set below was rebuilt from scratch.

### The whole set: all 45 configurations

`prebuild.py --parallel 4 --metal` over `out/c-fx/requests` (all configurations), fresh
cache with patch 0005:

| Round (attempt) | Jobs | Published | Wall time |
| --- | ---: | ---: | ---: |
| 0 `vs_1_1/ps_1_3` | 6,634 | 667 (+3) | 337 s |
| 1 `vs_1_1/ps_1_4` | 5,964 | 1,488 | 330 s |
| 2 `vs_1_1/ps_2_a` | 4,476 | 4,416 | 1,988 s |
| 3 `vs_1_1/ps_2_b` | 60 | 36 | 477 s |
| **Total** | **17,134** | **6,607** | **52 min** |

(+3): the three `.mfx` metadata jobs (`ra_metadata` RaShaderSM/STM/BM) publish reflection-only
records; a 2026-11-12 rerun of round 0 counts them (670). They are not D3DX acceptance outcomes and
`refusal_table.py` drops them. That rerun's other 6,631 refusal keys match `d3dx-refusals.txt`
exactly (0 differing outcomes).

### ATI=1: the same set on an AMD Mac (2026-11-12)

The adapter reports the real GPU vendor (owner decision), so an AMD Mac forms `ATI 1` requests
(Ra `RaDefines.fx` and the ShaderManager vocabulary). They were regenerated and merged into the
same table:

```sh
python3 tools/macos/d3dxfx/enumerate_requests.py --caps metal-ati --classes client,conditional,unrecovered \
  --output out/c-fx/requests-ati
python3 tools/macos/d3dxfx/prebuild.py --requests out/c-fx/requests-ati --corpus out/c-fx/corpus \
  --cache out/c-fx/cache-ati --work out/c-fx/prebuild-ati --parallel 4 --max-rounds 4
sh tools/macos/nativefx/build.sh        # out/build/nativefx/tool/nativefx (refusal-keys)
python3 tools/macos/d3dxfx/refusal_table.py --state out/c-fx/prebuild-ati/state.json \
  --requests out/c-fx/cache-ati/requests --merge-with Code/BF2/Platform/Gfx/Effect/data/d3dx-refusals.txt \
  --note "..." --output Code/BF2/Platform/Gfx/Effect/data/d3dx-refusals.txt
```

| Round (attempt) | Jobs | Published | Wall time |
| --- | ---: | ---: | ---: |
| 0 `vs_1_1/ps_1_3` | 6,634 | 565 (incl. the 3 metadata records) | 268 s |
| 1 `vs_1_1/ps_1_4` | 6,069 | 1,593 | 184 s |
| 2 `vs_1_1/ps_2_a` | 4,476 | 4,416 | 936 s |
| 3 `vs_1_1/ps_2_b` | 60 | 36 | 208 s |

Round 0 publishes 105 jobs fewer than ATI=0: all 105 are RaShaderSM ps_1_3 permutations that D3DX
refuses with X3000 from the shipped `#if PSVERSION == 13 && ATI` / `FOOBAR` guard (per group: 531
groups published only at ATI=0, none only at ATI=1). The merge added 17,151 keys (1 already
present, identical; no conflict): 34,198 keys in all. The ps_2_a skip for ATI parts without
arbitrary swizzle never applies (the Metal caps set PS20Caps bit 0).

The 24 groups left after round 3 are gap 3's eight SkinnedMesh permutations in each of
the three lighting-high `RaDefines.fx` texts. The run was stopped there: the
single-configuration run already showed all ten attempts failing, and attempts 4 to 9
add 144 requests and, scaled from its 1,580 s for 8 groups, about 80 minutes of
failing compiles.

| Count | Value |
| --- | ---: |
| Requests formed (unique keys) | 17,134 (+144 not run, see above) |
| Compiled and published | 6,607 = every effect group except 3 metadata and 24 SM |
| Refused by D3DX | 10,527: X4502 3,516; X4512 1,764; X4532 1,542; X4520 1,014; X4500 1,014; X4510 906; X4516 444; X4507 93; X4523 90; E_OUTOFMEMORY 84; X4521 57; no techniques 3 |
| Parsed and translated to MSL | 6,607 of 6,607 (0 publication failures) |
| Metal libraries compiled | **17,471 of 17,471** (`newLibraryWithSource`, 34 s, no warnings) |
| Requests per configuration | 192 (L1S0) to 3,845 (L3S1); 82,224 summed over the 45 |

Engine behavior per class: every D3DX refusal, including `E_OUTOFMEMORY`, makes
`compileEffect` try the next attempt, exactly as on Windows; on macOS the refused key
is now a refusal record (cache format 2), so a warm cache records nothing. Groups that exhaust all ten attempts get
no effect (gap 10). The metadata class is gap 1. Legacy effects have one attempt; a
failure there leaves an unbound `Effect` in the ShaderManager cache
(`ShaderManager.cpp:797`); none failed.

### Size

| Part | Size | Needed at run time |
| --- | ---: | --- |
| `entries/*/entry.bin` (self-contained bundles) | 312 MiB | yes |
| `binary/` (FX2-input index, two per entry: flags and `| 0x800`) | 625 MiB | yes, for the engine's `.cfx` path |
| `entries/` sidecars (FX2, description, MSL, reflection) | 573 MiB | no |
| `requests/` (17,158 replayable snapshots) | 476 MiB | no |
| **Total cache** | **2.0 GiB** (0.94 GiB needed) | |
| Builder work directories | 3.4 GiB | no; delete after a build |

Per configuration (entry plus binary index): L3S1F3T3 about 195 MiB, the defaults
L2S1F2T2 about 100 MiB, L1S0F2T2 about 18 MiB. Each `binary/` record duplicates its
entry; storing a reference to the source entry would cut the run-time cache to about
0.33 GiB.

## Policy

The owner decided (2026-10-31, `docs/macos-port.md`, "Effect compilation policy") that the
final client runs **without Wine**: shaders are compiled natively in the client
(vkd3d-shader as a library, our own effect parser, MojoShader to MSL), and no prebuilt
cache ships. This package's Wine cache is therefore the **development route and the
reference oracle**, not the product. What carries over to the native route (c-native-fx):

- **The request set is content-independent.** Every request comes from the engine's fixed
  lists, the Shaders archive's directory scan and the `.mfx` metadata; template init
  builds every registered permutation whether a level uses it or not. Per-level work buys
  nothing; a native compiler sees the same 6,634 groups.
- **It depends on two setting pairs only**: Ra effects on (lighting, filter), 9
  `RaDefines.fx` texts, shadows selecting a subset; legacy effects on (filter, terrain).
  A native compile-on-load can key its own cache the same way.
- **Volume.** The heaviest configuration (L3S1F3T3) needs 3,845 compile attempts, of
  which 2,469 are refusals the engine expects. A native route must refuse the same
  attempts (a too-permissive compiler would make the engine settle on `ps_1_3`/`ps_1_4`
  variants that D3DX rejects), or the effects would differ from the oracle's.
- **Oracle use.** `enumerate_requests.py` names every request with its exact bytes;
  `prebuild.py` gives the D3DX verdict (published FX2/MSL or refusal class) per request,
  which the native output can be compared against.
- **The 8 SkinnedMesh permutations** of gap 3 cannot be produced by the dev route at all;
  they are listed in `tools/macos/d3dxfx/dev-route-limits.json` for c-native-fx.

For development, building the whole oracle set takes 52 minutes with
`prebuild.py --parallel 4` (0.94 GiB of run-time cache); one configuration's slice is
195 MiB or less.

## Cache format 2: refusal and reflection records

[CHOSEN, not recovered] Added after the first report, for the development route.

| Record | Written when | Runtime behavior (`Platform/Gfx/Metal/Effect.cpp`) |
| --- | --- | --- |
| Refusal, `refused/<key>.bin` | D3DX9_25 refused the request (any status other than "ok"); stores its `D3DXCreateEffectCompiler` and `CompileEffect` HRESULTs and error text | `load` returns `Error::Refused` and records **no** request. `D3DXCreateEffect` returns D3DX's HRESULT and error buffer; `D3DXCreateEffectCompiler` succeeds when D3DX parsed the source, and the compiler's `CompileEffect` then fails with D3DX's HRESULT and text. The engine moves to its next profile, exactly as with D3DX. |
| Reflection-only, `entries/<key>/entry.bin` | The source parses but `CompileEffect` fails with "There were no techniques" (the three `.mfx`). The builder compiles the same source plus one synthetic technique, describes that FX2, removes the technique and publishes the description without FX2 | `D3DXCreateEffectCompiler` returns a compiler with the full reflection: string arrays and their elements (`GetParameterElement` + `GetString`), literal annotations and their string-array elements. `CompileEffect` fails as D3DX did. `MetaFx::generate` no longer aborts. |

Supporting changes: `bf2_fx_cache_tool publish-refused`, `publish-reflection`,
`retarget`; `build-cache.py` publishes both records and has `--retarget`;
`Entry` gains `kind`, both HRESULTs and the log. The facade's `GetString` now resolves
string-array elements; before, MetaFx would have read empty strings without any error.

**Format and translator changes are fatal, with the fix in the message.** The cache format
is now 2 (`CacheVersion`, written to `identity.json` as `version`). Patch 0005 changed the
translator identity and the key schema includes the format, so every cache built before
this change must be rebuilt; none can be loaded. A client pointed at an older cache stops
at the first effect with:

```text
[FRONTIER] BF2FX_CACHE_STALE reached: <cache> has cache format 1, this client needs 2 (...).
Rebuild every recorded request with: python3 tools/macos/d3dxfx/build-cache.py --retarget --cache "<cache>" --work out/fxc-retarget
```

`--retarget` reads every `requests/*.request` snapshot (request envelopes of format 1
are still readable), rewrites its toolchain identity, deletes the old `entries/`,
`binary/` and `refused/`, and rebuilds all snapshots with Wine. Measured on a copy of
`out/fx-cache-s2-v1`: 11/11 requests rebuilt. Caches affected:

- `out/fx-cache-s2-*` in this and cloned worktrees (s2 fixtures): retarget before use.
  The G4 effect GPU test now finds its SwiffMenu/Font requests by content, not by key.
- **c-asan-heap's `out/fx-cache`** (its client runs): not touched here. After this
  branch merges, its client stops with `BF2FX_CACHE_STALE` until someone runs
  `build-cache.py --retarget --cache out/fx-cache` in that worktree. The command only
  writes below that worktree's `out/`.
- The default `~/Library/Application Support/OpenBF2/FxCache`: `build-cache.py` only
  writes below the worktree's `out/`, so build there and point `BF2_FX_CACHE` at it.
- This package's `out/c-fx/cache-all` (format 1). Its compiler results stand; the
  numbers above were measured with it.

Validation: s2 CPU suite 827 checks pass against a retargeted cache with one refusal and
three reflection records (new synthetic checks: refusal without HRESULT rejected, no
request recorded on a refusal, reflection record without FX2 alias, envelope version
mismatch). The G4 GPU test (`macos_metal_device --effect`) passes, including D3DX-exact
refusal failure on both creation paths with `requests.txt` unchanged, the BundledMesh
`.mfx` reflection (32 permutation strings, first and last element text, a literal's
`reqVertexElement` annotation elements), and the stale-format stop.

## Gaps

What the static walk cannot predict, or what the cache cannot hold yet:

1. **Metadata reflection: closed for the dev route** by reflection-only records (above).
   `MetaFx::generate` aborts when the compiler object cannot be created
   (`RaShaderPermutator.cpp:771`). The native route gets them from its own parser;
   `enumerate_requests.metafx` reproduces D3DX's array and literal counts for all three.
2. **Technique validation.** Offline, a published effect counts as valid. If the G4
   validator rejects a technique, the engine continues to the next attempt; those keys are
   in the enumerated upper bound but not prebuilt, and would be recorded as misses.
3. **SkinnedMesh permutations that never compile under Wine.** At lighting high
   (`RAPATH 0`, which enables `_USEHEMIMAP_ && RAPATH == 0` in `RaShaderSM.fx`), the
   eight SM permutations `hasnormalmap|usehemimap` with any of `objspacenormalmap`,
   `hasenvmap`, `hashemiocclusion` and **without** `hasshadow` fail all ten attempts with
   `E_OUTOFMEMORY` from `CompileEffect`; each attempt runs 1 to 4 minutes. Their shadowed
   twins resolve at `vs_1_1/ps_2_b`. A fresh, large-address-aware host (4 GiB) does not
   help. Retail BF2 is not known to lose these materials, so this is more likely Wine's heap or
   address-space behavior than an engine refusal [INFERRED]. This is a **dev-route
   limit**: the list is in `tools/macos/d3dxfx/dev-route-limits.json` for the native route.
   With the dev cache those skinned meshes draw nothing on lighting high.
4. **Device inputs.** Keys depend on the Metal caps (`Device.cpp`): shader versions,
   vendor 0, MaxAnisotropy, min/mag anisotropy, no multisample. `HASALPHA2MASK` and the A2M
   codes assume the transparency-AA probe finds nothing (`RendDX9Init.cpp:1495`: the
   `ATOC` format check and a GPU render probe). Any caps or probe change rewrites every Ra
   and legacy key; rerun the enumerator after a `Device.cpp` change.
5. **Legacy `RAPATH`.** The recovered `updateShaderDefines` pushes the literal `"0"`
   although its comment cites `itoa(g_Renderer[1740])` (`ShaderManager.cpp:214`). If the
   recovery is corrected, legacy keys for lighting 1 and 2 change (3 x the legacy set).
6. **Not enumerated:** debug defines (console `disable*`, `showChannel`), forced 1.4
   shaders, `GSQuickShaderPermutation`, partial precision, nvidia/ATI vendor paths, device
   reset restarts (same chain). Each is a different macro or `RaDefines.fx` text.
7. **Expansion pack and mods.** `mods/xpack` changes `m_name` (wrapper bytes), the water
   shader name and the archive; `--mod`/`--mod-name xpack` exist but were not built.
   Loose overrides under `mods/<mod>/shaders/` are refused rather than modeled.
8. **Unrecovered callers.** Rain, Lightning, PostProduction_nv3x and
   DebugD3DXMeshShapeShader are prebuilt from `RendDX9.dll` strings only. Names composed at
   run time (like `"Shaders/" + "SkinnedMesh"`) in unrecovered code are invisible to a
   string scan.
9. **Engine `.cfx` cache.** After the first successful compile the engine writes
   `@HOME@/mods/bf2/cache/<adapter GUID>_3153_<filter>/<stem>.cfx` and later creates the
   effect from those FX2 bytes; s2 publication already indexes FX2 inputs (flags and
   `| 0x800`). The `.cfx` directory ignores lighting, so a lighting change reuses the
   older build, as on Windows.
10. **Miss cost.** A Ra effect with no usable attempt is not cached as null:
    `RaShader::begin` (`RaShader.cpp:1138`) rebuilds on every draw and retries all ten
    attempts. With refusal records these are cheap refusals rather than recorded misses;
    on the native route the same holds for whatever it cannot compile.
