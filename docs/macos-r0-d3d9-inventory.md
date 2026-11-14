# Renderer inventory handoff

[CHOSEN, not recovered] Read-only analysis for the owner-requested macOS port,
following route 1 in [macos-port.md](macos-port.md#frontier-for-milestone-3-a-metal-renderer).
Baseline: `df2262e`, branch `mac/r0-d3d9-inventory`. No engine source, recovered
behavior, ABI, platform branch or reference installation was modified. No binary
recovery was attempted; IDA/Ghidra evidence is not needed for this source inventory.

## Result

[macos-d3d9-inventory.md](macos-d3d9-inventory.md) contains interface and method
counts, constants, D3DX groups, top files, raw pointer crossings, separately
reported outside sources, unresolved examples and proposed seam groups. The
proposal preserves today's individual methods and arguments for a Windows D3D9
backend before a Metal implementation.

The requested directory trees contain **812 lexical call sites**: **515 D3D9/COM
sites or unresolved candidates**, and **297 D3DX sites**. Of these, **776** belong
to the renderer source/header/inline set and **36** to the sibling
TextureAtlasBuilder DLL (28 D3D9, 8 D3DX). The scanner also reports **382** sites
outside those trees; these are excluded from the renderer totals. Outside sources
include editor tools, fixtures and vendor backends, including an Xbox backend
that must not be treated as a D3D9 renderer dependency.

The scanner includes direct COM calls, local SDK interface prefixes, effect-slot
adapters, immediate raw effect calls, ID3DXInclude callbacks and two dynamically
loaded D3DXCreateEffectCompiler calls. It scans inactive platform/editor arms,
headers and inline files without expanding template instantiations or includes.
Arguments retain string/numeric literals, enums, casts and expressions. Extra
constant and engine-forwarding-helper CSVs retain state IDs and flags that would
otherwise be hidden behind variable arguments at the final device call.

## Reproduction and artifacts

From any directory, run the repository's script with Python 3:

```sh
python3 tools/macos/d3d9_inventory.py
```

Only Python's standard library is used. No SDK, compilation database, installed
package, game content, reference binary or network access is required. `--out`
and `--report` override output destinations. Outputs under `out/` are intentionally
Git-ignored; the coordinator can regenerate them without staging generated data.

| File | Contents |
| --- | --- |
| `tools/macos/d3d9_inventory.py` | Deterministic scanner and report generator |
| `docs/macos-d3d9-inventory.md` | Generated inventory and seam proposal |
| `docs/macos-r0-d3d9-inventory.md` | This validation/handoff record |
| `out/inventory/d3d9_calls.csv` | One row per D3D9/COM call or unresolved candidate |
| `out/inventory/d3dx_calls.csv` | One row per D3DX call, including slot dispatch |
| `out/inventory/d3d9_constants.csv` | Code-token occurrences with source locations |
| `out/inventory/d3d9_crossings.csv` | Typed declarations, opaque annotations, pointer overlays and untyped member candidates |
| `out/inventory/d3d9_wrapper_calls.csv` | Supplementary engine forwarding call arguments, excluded from COM totals |
| `out/inventory/manifest.json` | Scanned source hashes, roots, totals and sampling seed |
| `out/inventory/coverage.json` | Independent member-syntax, effect-slot and import-name coverage checks |
| `out/inventory/spot_checks.csv` | Twenty sampled rows, seed 1509 |
| `out/inventory/spot_checks.txt` | Numbered original-source excerpts for the sampled rows |
| `out/inventory/validation.json` | Local repeated-run results and artifact hashes |

## Validation

- Two consecutive full scans produced byte-identical reports, CSVs, source
  manifests and sample excerpts. Runs take about 4–5 seconds on this Intel Mac;
  299 source files were scanned. Exact timings and output hashes are retained in
  `out/inventory/validation.json`.
- A focused in-memory C++ fixture checked 11 calls: ignored comments and free
  function declarations, array receivers, explicit casts, literal string
  arguments, SDK-prefix aliases, effect/buffer raw dispatch and unresolved
  receivers. All assertions passed. No fixture engine source was written.
- The 20 rows selected with `random.Random(1509)` were read against their source
  lines and receiver declarations/contracts. **20/20 matched** location,
  interface/method and arguments. This included texture/surface operations,
  device state and format checks, mesh locking, effect parameter calls, shader
  assembly and IUnknown release. The MetaFx annotation helper receives the
  ID3DXEffectCompiler result, rather than an ID3DXEffect; its classification
  follows `MetaFx::generate -> readStringArrays` in the source.
- Source-call coverage was checked against the D3DX import definition and the
  raw dispatch forms in RenderQueues, ShaderManager, StaticMesh, SkinnedMesh,
  EffectManager and RaShaderPermutator. This caught the dynamic compiler calls,
  inline buffer-slot signatures, effect record binders and include callbacks.
  The reproducible `coverage.json` check finds **zero unmatched capitalized
  member-call sites**, **136/136** raw `d3dxSlot` dispatches and call sites for
  **all 30** names in `D3dx9_25.def`.
- No C++ build or server test was run: the only executable change is an offline
  Python analysis tool. No renderer startup, Windows D3D9 backend behavior,
  GPU output, gameplay or Metal path was exercised.

## Open boundaries

**Ten sites remain unresolved**, and all ten are listed with file/line in the
inventory. Eight are `self->buffer` calls in `BufferManager.cpp`: both vertex
and index wrappers declare that member name. The lexical resolver deliberately
does not infer the enclosing function's owner type. Their methods and complete
arguments are retained. Two are ShaderManager's templated effect walks, where
the slot is supplied by the caller. The source instantiates these walks with
`D3dxOnLostDeviceFn` / `0x114` and `D3dxOnResetDeviceFn` / `0x118`; the physical
dispatch rows remain `slot:dynamic` rather than being multiplied per instantiation.

This tool is not a C++ compiler: cross-file name resolution is unambiguous lexical
declaration matching, not overload/member lookup. All void-pointer members are
retained as candidates; some are engine objects or vtables. Integer-encoded
resource pointers, aliases without type/annotation evidence, and arbitrary
macro/raw-vtable dispatch cannot be proven exhaustively by this scanner. The
StateManager texture cells and void-pointer resource mirrors are explicitly
called out in the report. The coordinator should review opaque candidates and
these ten unresolved rows when designing the seam, keeping lifetime and query
behavior alongside draw/state calls.

Existing production frontiers remain unchanged, including `RW-56`'s renderer
overlay, `Effect::~Effect`, ShaderManager's states-effect commit arm and
RaStreamDataEditable resource-tracker registration/removal. The inventory covers
current source calls; it cannot supply calls inside bodies that remain unwritten.
Windows wrapper validation and subsequent Metal implementation remain future work.
