# RendDX9_r: the 2006 debug-asserts renderer as evidence

## What this evidence is, and its limits

`RendDX9_r.dll` is a DICE BoosterPack2 build of the renderer, linked 2006-05-04 (around patch 1.3),
PDB path `D:\DiceCanada\BoosterPack2\bin\RendDX9_r.pdb` (PDB not available). It is the same source
tree as the retail 1.50 `RendDX9.dll`, two years older, built with asserts on and incremental
linking. It is read-only, game-derived evidence under `out/rend-evidence/debug-r/` and is never
committed.

Rules for using it (AGENTS.md vocabulary):

- **1.50 instructions remain the source of truth.** An `_r` body is a hint about an older version,
  never proof of the 1.50 body. Do not copy `_r` behaviour, field offsets or vtable slots into the
  1.50 reconstruction without reading the 1.50 instructions; LightManager's vtable, for example,
  has 196 slots in `_r` and 189 in 1.50, and identical bodies call different slot offsets.
- An `_r` `__FILE__` attests the **file** of the `_r` function that contains the Debug site. For a
  1.50 function it is evidence only through a match in `rdebug-map.csv`; use `high` pairs only.
  Where such a file disagrees with a placement guess, the attested `__FILE__` wins (AGENTS.md),
  but a file renamed between 1.3 and 1.50 is possible, so a contradiction is a task for review, not
  an automatic move.
- An `_r` assert expression names members (`mRenderView`, `m_tempBackBuffer`, ...) as they were
  spelled in 2006. Field names are hints; offsets must come from 1.50.
- File placement taken from neighbouring `_r` functions (`file_basis=neighbour`) is `[INFERRED]`.

## What `_r` contains

- `.text` 4.2 MB (1.50: 1.9 MB). Incremental link: calls and vtable slots go through `jmp` thunks
  at 0x10001000..; there is no identical-code folding, so every template instance has its own
  body. Imports the `_r` sibling DLLs `memory_r.dll`, `pcregexp_r.dll`, `textureatlasbuilder_r.dll`.
  Exports only `initDll`/`deinitDll`. RTTI: only the six CRT/STL type descriptors
  (`exception`, `bad_cast`, `logic_error`, `length_error`, `out_of_range`, `type_info`); no class
  RTTI, no symbols.
- 178 distinct `Code/BF2/...` source paths (158 `.cpp`, 20 headers/`.inl`). 1.50 itself keeps 94.
- 2048 `dice::hfe::Debug` sites. `_r` calls the non-inlined `Debug::Debug(type, file, line, func)`
  at 0x1011D2B0 and `Debug::~Debug` at 0x1011D8F0; layout `{type +0, std::string file +4,
  std::string func +0x20, int line +0x3C, ostringstream +0x40}` (MSVC 7.1 strings, 0x1C each). 1.50
  uses the same pair at 0x10074300/0x10074690 for its 288 retained Error/Info sites. By type:
  Assert 1189 (+2 type 4), DxAssert 38, Error 293 (+7 type 7), Warning 148, Info 75, Debug 267,
  27 with the type in a register. The `func` argument carries the assert expression: 553 distinct
  expressions, 309 of them naming a member (`m_...` or `->`). Non-assert sites carry the streamed
  message literals instead. (The main session's count of about 268 assert-like strings counts
  strings; this table counts call sites. Its 159 `.cpp` paths agree with the 158 here, which
  folds one spelling variant.)
- Every site, with its file, line, expression, streamed literals, containing `_r` function and the
  matched 1.50 function: `rdebug-asserts.csv` (an archived working table; 2048 rows; `status` is `ok` for
  1988, otherwise the reason the argument backtrace stopped).

### EditorRendDX9.dll (2006-05-15)

The editor build (`D:\bf2editor\bin\EditorRendDX9.pdb`, 6 MB) adds no symbols, no exports beyond
`initDll`/`deinitDll`, and the same six RTTI descriptors. It has 160 paths. It has two paths that
`_r` lacks: `RendDX9/TextureAtlas/EditorTextureAtlasBuilder/{EditorTextureAtlas,Packer}.cpp`. It
lacks 20 that `_r` has, including Rain, Lightning, every `XpLight*`, `LightMerger`,
`SkinnedMeshRenderer`, `Scroller`, `RaQuadPatchAdapter`, `DecalShadowManager` and `RenderStats`. It
is an older or separate branch, not a superset of `_r`. Its Debug pair is at 0x10160140/0x10160780,
with 1855 sites. It has 26 assert expressions that `_r` lacks, each with its file and line:

    ShaderManager.cpp:147 pD3DXEffect; :274 iRead == iFileSize && !pStream->getAvailableBytes()
    StateManager.cpp:692 Sampler < 8;  StateManager.h:88 SIZE < (1<<(8*sizeof(Inx)))
    FSQuadDrawer.cpp:363 m_nextOffset/4 < 32;  RaShader.cpp:352 0
    BundledMeshTemplate.cpp:1193..1220 m_paramParaboloidZValues, m_paramParaboloidValues,
        m_paramLightPosition, m_paramAlphaThreshold;  BundledMesh.cpp:951 m_shadow
    Terrain.cpp:2271 sizeof(indices[0]) == sizeof(U16);  StaticMesh.cpp:702 viewMat && projMat
    SkinnedMesh.cpp:1309 bf2renderer->m_pGBuffers[0];  NametagManager.cpp:2041 it != m_items.end()
    RoadEditable.cpp:2594 pPoints;  LightManagerGetSets.cpp:518 true;  TerrainEditable.cpp:2783 1
    DecalRenderer.cpp:604 (m_prevIndex + batchCount) < MaxDecalCountTotal;
        :620 m_decalBatchCount < MaxDecalBatchCount
    DecalObject.cpp:1015 decal->m_geomVertexCount < MaxProjectedVertexCount; :1086, :1093 (lod/face)
    ShaderManager.cpp:894, EffectManager.cpp:1078 ((HRESULT)(hr) >= 0)

It is not matched function-by-function. Use `debug_sites.py` with ctor `0x10160140` and dtor
`0x10160780` to list its sites.

## Function match (`docs/tracks/rend/rdebug-map.csv`)

Columns: `addr_150,addr_r,confidence,method,file,line_range,file_basis`. `file` is the `_r` file
(relative to `Code/BF2/`); `file_basis` is `site` (the `_r` function has its own `__FILE__`
site; `line_range` = its lines) or `neighbour` (the nearest attested `_r` functions on both sides
have the same `.cpp`: `[INFERRED]`).

Anchors, in order (see `tools/tracks/rdebug/match.py`): Debug sites of both images (file,
type, message), C strings that only one function in each image references, single-user imports,
then propagation by tied vtables (same slot), callee alignment, single unmatched callers,
link-order checks (both builds keep the object order) and equal matched-callee sets in a gap.
`high` needs two independent agreeing pieces of evidence and no conflicting vote. A single anchor is
`medium`, and weak evidence is `low` (for review only). A vtable or callee pair whose `_r` address
falls outside its high neighbours' bracket drops to `medium`.

Coverage:

| | pairs |
|---|---:|
| 1.50 functions (Ghidra 9762 without jump stubs, plus 1374 vtable-only, see below) | 11134 |
| matched, all confidences | 4293 |
| `high` | 2294 |
| `medium` / `low` | 1609 / 390 |
| ledger RendDX9 rows matched (of 9762) | 3646 (2076 high) |
| ledger `todo` rows with a `high` pair | 1360 |
| ledger `todo` rows that gain an `_r`-attested file (`high`, own `__FILE__` site) | 266 |
| ... of which: ledger had no file / agreed with its neighbour guess / disagreed | 146 / 78 / 42 |
| ledger `todo` rows with a `high` pair and a neighbour-bracketed `_r` file (`[INFERRED]`) | 360 |

Checks: all 176 `high` pairs whose 1.50 function has its own 1.50 `__FILE__` site name the same
file in both images (11/11 `medium`). 39 of 3903 high/medium pairs fall outside the link-order
bracket of their nearest high neighbours; none of those is `high`. Spot checks of the method
classes show matching instruction counts, constants and float sets.

Ledger gap found on the way: Ghidra's auto-analysis creates no function for 1374 1.50 entries that
are only reached through vtables (small virtual getters such as 0x10091B10, which
DebugTorusMesh.cpp already cites). They are missing from `ledger.csv`; 647 map rows (218 high)
are such functions. `features.py` finds them as vtable slot targets.

Reproduce (all outputs under the git-ignored `out/`):

    G="$GHIDRA_INSTALL_DIR/support/analyzeHeadless"
    $G out/rend-ghidra/proj-r renddx9r -import out/rend-evidence/debug-r/RendDX9_r.dll \
        -scriptPath tools/tracks/ghidra -postScript ExportFunctions.java out/rend-ghidra/RendDX9_r
    awk -F'\t' 'NR>1{print $1}' out/rend-ghidra/RendDX9_r/functions.tsv > out/rend-evidence/renddx9r-list.txt
    $G out/rend-ghidra/proj-r renddx9r -process RendDX9_r.dll -noanalysis -readOnly -scriptPath tools/tracks/ghidra \
        -postScript DecompPE.java out/rend-evidence/renddx9r-list.txt out/rend-evidence/RendDX9_r 60
    objdump -d --x86-asm-syntax=intel --no-show-raw-insn out/rend-evidence/debug-r/RendDX9_r.dll > out/rend-evidence/RendDX9_r.s
    R=tools/tracks/rdebug; E=out/rend-evidence; GAME=".../Battlefield 2/RendDX9.dll"
    python3 $R/debug_sites.py $E/debug-r/RendDX9_r.dll $E/RendDX9_r.s out/rend-ghidra/RendDX9_r/functions.tsv out/rdebug/sites_r.csv
    python3 $R/debug_sites.py "$GAME" $E/RendDX9.s out/rend-ghidra/RendDX9/functions.tsv out/rdebug/sites_150.csv 0x10074300 0x10074690
    RENDDX9_150="$GAME" python3 $R/match.py out out/rdebug/map.csv out/rdebug/sites_r.csv out/rdebug/sites_150.csv out/rdebug/asserts.csv
    python3 tools/tracks/rend_ledger.py --rdebug      # adds the r_file column (high, own site)

`rend_ledger.py --rdebug` appends an `r_file` column. Without the flag, the ledger and summary are
byte-identical to the previous script's output.

## Open items

### ObjectDrawer::flushDeferredGeometry (1.50 0x100F8C00)

- `_r` 0x102311F0 is the same function, in `Geom/ObjectDrawer.cpp`; its only assert is at line 719.
  The map lists it as `medium` (callee+order). The pair is `high` on a direct read: the
  instructions match apart from the assert prologue, with the same forward walk of the tree whose
  head is at +0x14 and the same reverse walk of the head at +0x20. Each walk makes the same
  `[[node+0x14]]+0x18` call with `(this+0x28, this+0x2C, 1)`, and both clears follow both walks.
  Its two callers 0x10231830/0x102318E0 match the 1.50 callers 0x100F8DE0/0x100F8F30.
- Field name: the assert is `mRenderView != 0` on `+0x28`, so the 2006 name is `mRenderView`, with
  `m`-without-underscore spelling. `+0x2C` is the occluder argument, not named.
- Other ObjectDrawer.cpp sites name no tree member. Two Error sites read `updateVisible() must be
  drawn before this!` (lines 598 and 638, `_r` 0x10231EA0/0x1022F710). The constructor 0x102327E0
  (line 55, `bRet`) builds both trees with head allocator 0x1022F250 and sets `+0x64 = 60.0f`.
  The profiler strings are `ObjectDrawer_UpdateVisible`/`objectDrawerUpdates`.
- **No insertion exists in `_r` either.** `_r` has no identical-code folding, so each node type
  has its own helpers: head allocator 0x1022F250, single-node erase 0x1022FB40, range erase
  0x10230F80 and recursive erase 0x1022F110. The whole image has eleven 0x1C-byte node allocations
  through the pool allocator 0x100F3920. For this node type the only one is the head-node allocator,
  so there is no `_Buynode` with a value and no `_Insert`. The value is 12 bytes (+0x0C..+0x17,
  mapped pointer at +0x14), but no code in either build writes it. The key type and comparator are
  therefore unrecoverable from `_r` as well. The trees are populated nowhere in the 1.3 build,
  which supports treating them as dead containers. Whether 1.50 still matches this remains a 1.50
  question: an insertion in 1.50 would have to be new code.

### GeometryTemplateManager slots +0x2C (0x1015D940) and +0x30 (0x1015D430)

- Same vtable (25 slots) in `_r` at 0x104A7A38. +0x2C is `_r` 0x102CA220 and +0x30 is
  0x102C9010; their instructions match 1.50 apart from addresses. +0x2C returns a static
  `std::vector` (0x1052A3FC) with an atexit freer 0x1040F580. +0x30 returns a static empty
  `std::string` (0x1052A3DC) and pops one unread stack word.
- No assert, no element access and no other reference to either static in `_r`. The vector's
  freer only releases storage (no element destructor loop), in `_r` as in 1.50, so the element
  type is trivially destructible (pointer or POD). That is a constraint, not a type.
- Placement: both sit among `Geom/GeometryTemplateManager.cpp` functions (neighbour
  `[INFERRED]`; that file's sites at lines 187-335 are in 0x102C96D0/0x102CA2A0, slots +0x50 and
  +0x3C). `_r` keeps slots +0x34/+0x38 as two distinct empty bodies (0x101DF0F0/0x101DF100),
  which 1.50 folds to 0x100C7EF8.

### Light update helper 0x100F2740 (called by Renderer::renderFrame at 0x1002FBED)

- `_r` 0x10227470 is the same function: 348 instructions in both, identical apart from
  addresses and two virtual slot offsets. 1.50 calls `+0x208`/`+0x200` and `_r` calls
  `+0x224`/`+0x21C` on the same receiver, the LightManager vtable, which has 189 slots in 1.50 and
  196 in `_r`. It sits in the same position in `_r`'s renderFrame (0x1008C180, `high` pair).
  The preceding adjustor thunk also matches: `_r` 0x10227460 = 1.50 0x100F2730 (`high`).
- File: inside the `_r` `Geom/LightManager.cpp` run (0x10224350..0x1022CE10), so
  `Geom/LightManager.cpp` (`[INFERRED]` from neighbours, no own site). The ledger guesses
  `Geom/XpLightManager.cpp`. The matcher missed this pair because its gap was not 1:1. The two
  getters it calls are 11-instruction bodies without asserts (`_r` 0x102C6F60/0x102C6F70) and stay
  unnamed.
- The surrounding LightManager class is `Geom/LightManager.cpp` in `_r`, not
  `XpLightManager.cpp`. Its methods have own sites there. enableLight 0x100F2CA0 (`_r`
  0x1022A2E0) is at lines 1196/1197, with asserts `m_lightSources.find(pLight) !=
  m_lightSources.end()` and `m_activeLightSources.find(pLight) == m_activeLightSources.end()`.
  disableLight 0x100F2BB0 (`_r` 0x10229670) is at line 1223, and loadSimpleShadowMaps 0x100F3B20
  is at lines 1326-1377. The light registration pair (`pLight->getClassID() ==
  CID_LightSource`, lines 761/792/804) is in the same file. The member names are
  `m_lightSources` and `m_activeLightSources`. `_r` also has `Geom/LightManagerGetSets.cpp`, `LightManagerUpdates.cpp`,
  `LightManagerUtils.cpp`, `LightMerger.cpp` and a separate `Geom/XpLightManager.cpp` that holds
  the `XpLightManager::` methods.

### `[INFERRED]` placements in Code/BF2/RendDX9, Geom, GeomMisc

File-level `[INFERRED]` placement headers (85 files checked):

- **Now attested** (the same path exists in `_r`): `Geom/DecalSystem/DecalManager.h`,
  `Geom/DecalSystem/DecalRenderer.cpp` (filename was `[CHOSEN]`), `Geom/NameTagRenderer.cpp`
  (`_r` spells it `NametagRenderer.cpp`), `Geom/NametagManager.h`, `Geom/SkyDome.cpp`,
  `Geom/StaticMeshTemplate.h`, `Geom/TerrainEditableGetSets.cpp`, `GeomMisc/DebugTextWriter.cpp`,
  `GeomMisc/FontSystem/FontSystem.cpp`, `RendDX9/PostProductionFrameWork.cpp`,
  `RendDX9/StateManager.cpp`.
- **Contradicted** (same file name, other directory in `_r`):
  `Geom/SwiffRender.cpp` -> `Geom/SwiffRender/SwiffRender.cpp` (also `Scroller.cpp`,
  `TextureCacheManager.cpp` there); `GeomMisc/MenuRend/FSMoviePlayer.cpp` ->
  `GeomMisc/FSMoviePlayer.cpp`; `GeomMisc/MenuRend/Video.cpp` -> `Geom/VideoRender/Video.cpp`
  (VideoTarget::create 0x1015A6D0 -> `Geom/VideoRender/VideoTarget.cpp`);
  `GeomMisc/OverlayTexture.cpp` -> `GeomMisc/FontRenderer/OverlayTexture.cpp` (setTexture
  0x10088100, own site).
- `Geom/LightningManager.cpp` / `WeatherManager.cpp` ("no `__FILE__`"): `_r` has
  `Geom/Lightning/Lightning.cpp` and `Geom/Rain/RainManager.cpp`; not matched to the 1.50 bodies
  here, so they remain `[INFERRED]` candidates.
- The other 70 file-level claims (mostly headers, consoles and `.inl` splits) name no path that `_r`
  has. They stay `[INFERRED]`.

Function-level placements with a PE address (`high` pairs with an own `_r` site unless noted):

- `RendDX9/BF2Render.cpp:5651` selectShaderArchive 0x10042CD0 ("beside its only caller"): `_r`
  `RendDX9/Ra/RaShaderServer.cpp`.
- `RendDX9/RendDX9Draw.cpp:185` screenshot run: 0x10005640 agrees (RendDX9Draw.cpp); the capture
  body rendererCaptureScreen 0x10005200 is `RendDX9/RendDX9.cpp` in `_r`.
- `Geom/QuadPatch.cpp` TerrainCuller bodies (0x100E2D80..0x100E68B0): `_r` 0x102086A0..0x1020DAB0
  has no site of its own. The run sits after the last `Geom/Terrain.cpp` site and before
  `Geom/BundledMeshTemplate.cpp`, in both images, and far from `QuadPatch.cpp`. That file's
  line-1671 Error is at 1.50 0x101885E0 and `_r` 0x102FF8B0. Not attested; link order points to
  `Terrain.cpp` or an assert-free `TerrainCuller.cpp`, not `QuadPatch.cpp`.
- `.inl`/header splits marked `[INFERRED]`/`[CHOSEN]` and the `_r` TU of their bodies:
  ParticleRendererTrail.inl -> `Geom/ParticleSystem/TrailRenderer.cpp`;
  ParticleRendererGpu.inl, ParticleRendererCallbacks.h -> `ParticleSystemRenderer.cpp`;
  ParticleRendererNsa.inl -> `NSAParticleSystemRenderer.cpp` (medium);
  ParticleTemplateResources.h -> `ParticleSystemTemplate.cpp` / `NSAParticleSystemTemplate.cpp`;
  BundledMeshMaterial.h (0x100E9520/0x100E9840) -> `Geom/BundledMeshTemplate.cpp`;
  BundledMeshDraw.cpp 0x10104F70 -> `Geom/BundledMesh.cpp` (medium);
  StaticMesh.cpp:2629 0x100E29D0 -> `Geom/Terrain.cpp` (neighbour).

Ledger files that `high` own-site pairs contradict, among non-inferred `done`/`stop` rows (a
work list for the coordinator; the full list comes from `rend_ledger.py --rdebug`):

- `Geom/XpLightManager.cpp` LightManager methods: these belong to `Geom/LightManager.cpp`,
  `LightManagerGetSets.cpp` and `LightManagerUpdates.cpp` (enable/disableLight,
  setDynamicShadowsEnabled 0x1015CEC0, buildShadowCandidateSet 0x10164DC0). fillNormalCubeMip
  0x101C3B00 belongs to `RendMisc/BumpmapUtil.cpp`.
- `Geom/TerrainDraw.cpp`: 7 bodies (drawMain 0x100E0940, lockDynamicIndexBuffer 0x100D8570, ...)
  are in `Geom/Terrain.cpp`.
- `Geom/NametagManagerUpdate.inl`: 5 bodies are in `Geom/NametagManager.cpp`.
  `NameTagRendererDraw.inl`: 2 bodies are in `Geom/NametagRenderer.cpp`.
- `Geom/UndergrowthSystemLoad.cpp`: 3 bodies are in `Geom/UndergrowthSystem.cpp`; saveDat
  0x10146610 is in `io/StreamUtil.inl` (inlined helper site).
- `Geom/SkinnedMesh.cpp`: findShaderForBits 0x1010EFF0 and skinnedMeshInitShadowState 0x1010FD00
  are `Geom/SkinnedMeshTemplate.cpp`. 1.50's own Debug site in 0x1010EFF0 already says so.
- `Geom/StaticMesh.cpp`: lightmap name helpers 0x10197F00/0x10198010/0x10198410 are
  `Geom/StaticMeshLightmapGeneration.cpp`.
- `GeomMisc/FontSystem/FontSystem.cpp`: FontPorted 0x10087560/0x100876A0/0x10087A90 are
  `GeomMisc/FontRenderer/PortedFontRend.cpp`. `SwiffFont.cpp`: FontRenderer::init 0x100A1240 is
  `GeomMisc/FontSystem/FontRenderer.cpp`, FontSystem::loadFont 0x100A0480 is `FontSystem.cpp`,
  and DifFont::load 0x100A4420 is `Font.cpp`.
- `RendDX9/RendDX9Init.cpp`: device listener broadcasts 0x10002530/0x10002590/0x10002600 are
  `RendDX9/DeviceStateServer.cpp`, and registerRenderPathListener 0x1000D170 is
  `RenderPathServer.cpp`.
- `RendDX9/BufferManager.cpp`: vertexBufferUnlock/BindStream are `RendDX9/VertexBuffer.cpp`, and
  indexBufferLock is `IndexBuffer.cpp`.
- `RendDX9/StateManager.cpp`: rendererDrawIndexedPrimitive/DrawPrimitive 0x10005820/0x10005870
  are `RendDX9/RendDX9Draw.cpp`.
- `RendDX9/RenderQueues.cpp`: raShaderConstantsConstructInPlace 0x10038BF0 is
  `Ra/RaGlobalShadingSettings.cpp`, and createRaShaderConstants 0x1003ACA0 is
  `Ra/RaShaderManager.cpp`.
- `Geom/Terrain.cpp` / `Terrain.h` TerrainRawReader read/seek/tell 0x10075E20/40/50 are
  `IO/FileUtil.cpp`. 0x1013A260 (`TerrainRawReader::writeU32` in the ledger) is
  `Geom/RoadTemplate.cpp`; the ledger symbol for that address is suspect.
- Single rows: queryReadOcclusionResult 0x10004050 is `RendDX9/Query.cpp`, `SkyDome.cpp` claims
  renderFrame's address 0x1002F7E0, which is `BF2Render.cpp`, OverGrowth::generateAtlas is
  `OverGrowth.cpp`, acquireGeometry 0x1015EE40 is `Overgrowth/Growth.cpp`,
  QuadPatchDetailLayer::load 0x101BB7C0 is `Geom/DetailChart.cpp`, and
  particleSystemType1ResolveGeometry 0x10179080 is `MeshParticleSystemTemplate.cpp`.

## Is the IDA database needed?

No, not for steps 1-4. Everything above comes from the DLL itself. `RendDX9_r.dll.i64` is worth
reading only for the owner's manual work from 2025-10-10: renamed functions, comments and types.
If the coordinator wants it, the useful export is a name list. In the Boot Camp VM, run an IDC
script over `get_next_func` that prints `address<TAB>name` for every function whose name does not
start with `sub_`, and every non-default global name. Also export user comments if possible
(`get_cmt`). Copy the TSV to `out/rend-evidence/debug-r/`; `match.py`'s map then carries
those names onto 1.50 addresses.

## IDA databases (.i64)

The owner confirmed on 2026-11-04 that the `RendDX9_r.dll.i64` and `BF2_r.exe.i64` databases hold almost
no manual work (no meaningful renames), so they add nothing beyond `RendDX9_r.dll` itself. Ignore them.
