# Tests and fixtures

Start with the validation commands in the [development guide](../../docs/development.md).
`syntaxcheck.cmd` checks engine translation units; the CMake `bf2_run_checks` and
`bf2_editor_run_checks` targets build and run focused suites. Individual `.cmd` files
build the matching component fixtures. `aidll/` exercises the AI image; `tier2/`
and `tier3/` cover content loading and vehicle behavior.

Fixtures may supply typed observers or isolated copies of production translation
units. They are test scaffolding and do not replace the production implementation.
Report the paths exercised and keep generated outputs in an isolated build directory.
Runtime checks must use explicitly configured ports other than 16567.

Some checks need separately supplied retail content or previously built objects:

- Set `BF2_EDITOR_KARKAND_DIR` to an extracted stock `Strike_at_Karkand` level
  directory to enable the editor's optional terrain and detail-map checks.
- Prefer `tier2/run.py` for aircraft regression coverage. The historical
  `aircraft_runtime.cmd` observer requires `BF2_BUILD_OUT` and
  `BF2_AIRCRAFT_FIXTURE_DIR`, containing its instrumented `bootmain.cpp` and
  `stock_expectations.tsv`; it writes its outputs into that fixture directory.
- `effect_stock_d3d_runtime.cmd` requires `BF2_EFFECT_FIXTURE_DIR`, containing
  the fixture's matching `native_obj` build and `stock_PortedMenuShader.fx`.
  It writes results under that directory's `d3d_runtime` subdirectory.

Keep game content and original binaries outside the source tree. Missing optional
content checks report a skip; scripts requiring external inputs fail when they are absent.

Additional component fixture inputs:

- `collision_teardown_runtime.cmd` requires `BF2_BUILD_OUT` from a completed dedicated
  build and `BF2_COLLISION_MESH`, the absolute path to a stock `flagpole.collisionmesh`.
  Results go under `BF2_BUILD_OUT/collision_teardown_runtime`.
- `hud_atlas_snapshot_runtime.cmd` requires `BF2_HUD_ATLAS_FIXTURE_DIR` with the fixture's
  matching 49-TU native renderer build in `native_obj/`. This object set must include
  `49_QuadDrawer.obj`, `15_TextureManager.obj` and `binkw32.lib`; D3DX links from the
  April 2005 DirectX SDK that `Code\Build\dxsdk.cmd` locates. The fixture supplies its own
  TextureManager implementation at link time. Results go under that fixture directory's
  `atlas_snapshot_runtime/` subdirectory.

Use separate fixture and output directories for concurrent runs. Both scripts report missing
required inputs and exit before compilation.
