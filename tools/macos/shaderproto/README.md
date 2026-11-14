# macOS shader pipeline experiment

**Developer oracle, not used by the client.** This optional historical experiment
is outside the client build. The production compiler is the
native effect route ([building](../../../docs/building.md#macos)). The native dependency
builder never invokes this directory's tools.

[CHOSEN, not recovered] Standalone tooling for the owner-requested experiment
(the S0 shader prototype). No engine target uses it.

Run from the repository root, using the already populated `out/third_party/` and
`out/fx-sample/src/`. Nothing downloads, installs, starts a server, or writes game content.

```sh
sh tools/macos/shaderproto/build.sh
python3 tools/macos/shaderproto/sweep.py
python3 tools/macos/shaderproto/permutations.py
python3 tools/macos/shaderproto/entries.py
python3 tools/macos/shaderproto/sweep.py --source Code/Tests/macos_shaderproto --output out/shaderproto/controls
```

The CMake project uses Unix Makefiles and builds only the Objective-C++ harness.
Libraries build under `out/third_party/build/`; the harness builds under
`out/build/s0-shader-proto/`. `build.sh` generates upstream headers with the local
MinGW Wine IDL compiler. Set `WIDL` if that executable has moved.

One effect with includes and optional defines:

```sh
out/build/s0-shader-proto/shaderproto out/fx-sample/src/SwiffMenu.fx out/fx-sample/src out/shaderproto/one PSVERSION=20
```

The include resolver maps `shaders/`, `mods/bf2/shaders/` and
`mods/xpack/shaders/` onto the supplied extracted archive, case-insensitively.
This is a prototype search policy, not the engine's mod layering implementation.
Absent includes fail visibly. Inputs are never rewritten. Set a different
`--output` for sweep variants; default output is `out/shaderproto/results.csv`.

Each effect run writes a JSON result, diagnostics and, when stages succeed, an FX
binary, numeric parameter/pass/state inventory and one `.metal` file per shader
object. Pass state shader-object indices connect passes to shared shader objects.
The effect API uses a BYTECODE parsing callback first; MSL translation runs only
after the complete effect parses. Runtime Metal compilation checks the named entry
exists in the library. No draw, render pipeline or effect execution is tested.

The CSV's `stage_reached` is the last **attempted** stage. The four `*_ok` columns
record successes; later stages remain false when an earlier stage fails. Metadata
with zero programmable shaders cannot pass translation or Metal. Each input runs
in a child process, so a compiler trap cannot stop collection. Full diagnostics are
in `effects/<filename>/stderr.log` and `compile.log`. Source hashes, commands and
defines are retained under `out/`; game content is never added to Git.

`permutations.py` builds three wrappers from `.mfx` literals and annotations using
the current `MetaFx::generateShader` algorithm and engine define tables. These are
separate from the empty-define sweep. `entries.py` tests SwiffMenu's literal shader
entry/profile pairs individually, bypassing FX serialization and effect parsing.
Its results must never be counted as full effect successes.

For a direct downstream diagnostic:

```sh
out/build/s0-shader-proto/shaderproto --shader vs_1_1 VSShape out/fx-sample/src/SwiffMenu.fx out/fx-sample/src out/shaderproto/swiff-vertex
out/build/s0-shader-proto/shaderproto --shader ps_2_0 Pixel Code/Tests/macos_shaderproto/control.fx Code/Tests/macos_shaderproto out/shaderproto/control-pixel
```

`--shader` selects HLSL → D3D bytecode and enables vkd3d's documented
`allow-half-globals` compatibility option. In this mode `parse_ok` means shader
bytecode parsing, not effect parsing; JSON explicitly records the mode. Effect
mode always uses default compiler options and `fx_2_0` → `TARGET_FX`.

The harness returns 0 only when every emitted shader compiles to a Metal library
and entry point; failures return 1, usage/I/O errors return 2, compiler traps retain
their signal. The Python collectors return 0 when collection finishes, regardless
of shader results; inspect the CSV and summary counts for acceptance. Synthetic
fixtures in `Code/Tests/macos_shaderproto/` are tool controls, not game replacements.
