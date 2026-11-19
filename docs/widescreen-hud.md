# Widescreen HUD and rendering fixes

This is the intentional client change proposed in
[OpenBF2 issue #33](https://github.com/kr4uzi/OpenBF2/issues/33), marked
`[CHOSEN, not recovered]`. It is not a claim about recovered BF2 1.50 behavior.

## Credit

The HUD design, vanilla node lists, wider FOV cap and water-buffer guard come from
[marekzajac97/BF2-HUD-fix](https://github.com/marekzajac97/BF2-HUD-fix), examined at
[af6cbe6](https://github.com/marekzajac97/BF2-HUD-fix/tree/af6cbe6ab4bb0947c84b595e3cb3bf5ec5e1d75b).
That project credits BadSanta12345's Alpha Project HUD fix as its inspiration.
BF2-HUD-fix and this repository use GPL-3.0; its policies are adapted here with
attribution. Its ASI loader, fixed-address hooks and binary wrappers are not needed
in a source build.

## Use and scope

The rebuilt client applies this layout automatically when it starts vanilla
`mods/bf2`, including on a mod restart. Special Forces and custom mods use their
own layouts. See the [decision on issue #45](https://github.com/kr4uzi/OpenBF2/issues/45#issuecomment-6984306389).

Vanilla BF2 widens the HUD projection to match the active pixel viewport while
keeping its height at 600 logical units. Ratios at or below 4:3 retain the 800×600
projection. It does not alter the front-end menu or change archives.
World-marker/high-DPI behavior is outside this scope and must be checked in the
native visual acceptance pass.

Two renderer changes apply in all mods:

- `RenderView::setAdjustedFov` raises the wide-screen cap from 16:9 to 2560:1080.
  At 1920×1080 the result is unchanged; 2560×1080 keeps the requested vertical
  FOV instead of multiplying it by 0.75. Wider ratios still reduce it: at
  3840×1080 the multiplier becomes 2/3 instead of 1/2. The existing behavior at
  5:4 and taller ratios stays intact; the donor's optional `fov_scale` is not added.
- `Terrain::drawWaterCell` skips cells once the current pass has filled its
  256-entry parameter buffer. It preserves queued entries without wrapping or
  writing beyond the buffer. Excess cells remain undrawn, so water can be missing
  when the limit is reached. `drawWaterGrid` still resets the cursor each pass.

## Implementation contracts

- `HudWidescreen.cpp` owns the per-System policy, vanilla node lists and
  mod gate. A closed/destroyed HUD loses its state; a recreated HUD starts fresh.
- `Ref2System` samples the active render view before HUD input, animation and
  paint. Zero/invalid viewports retain the last valid width during minimization.
  The authored System root stays 800×600, so center panels and crosshairs retain
  their original coordinates.
- `Bf2HudBuilder` creates left/right siblings at the vanilla Chat anchor and
  reparents the named vanilla groups. Movable split groups become 800×600
  transforms. This uses the native canvas dimensions rather than copying the
  donor hook's swapped constructor dimensions.
- `TransformNode` and `Bf2TransformNode` use the same optional bounds filter for
  painting, visiting and hit testing. It never writes authored node geometry;
  recomputing bounds cannot accumulate offsets across frames or resolutions.
- The collapsed minimap animates toward its right-edge anchor; expanded maps
  retain centered targets. Crosshair maximum deviation retains its clamp and
  authored horizontal center.

There are no new virtual methods, object fields, packet fields, or file formats.
The Ref2System refresh helper is nonvirtual. The layout callback is native
scaffolding, disabled until the in-game HUD attaches; other UI systems do not
opt in. Existing unknown Node suffix slots and publication frontiers remain
unchanged, including `node-native-prefix-publication`, `node-suffix-call-frontiers`
and `bf2-picture-native-accessors`. This change supplies no missing recovery bodies.

## Files for local review

Paths below are relative to the repository root.

| Files | Responsibility |
| --- | --- |
| `Code/BF2/Menu/HudWidescreen.{h,cpp}` | HUD layout policy, attribution, vanilla node lists and lifecycle |
| `Code/BF2/Game/Main/BF2Engine.cpp` | Select the HUD policy from the active mod at startup |
| `Code/BF2/Menu/Ref2System.{h,cpp}` | Viewport refresh, projection and teardown |
| `Code/BF2/Menu/BF2HudManager{Init,Input,Update}.cpp` | Attach the HUD, refresh before animation/input, extend pointer bounds |
| `Code/BF2/Menu/Bf2HudBuilder.cpp` | Named anchor construction and parenting |
| `Code/BF2/External/BFMeme/TransformLayout.h`, `TransformNode.cpp` | Native bounds-filter seam and shared traversal calculation |
| `Code/BF2/Menu/Bf2TransformNode.cpp` | Apply the same policy to HUD hit testing |
| `Code/BF2/Menu/Bf2MenuMapNode.cpp`, `HudAmmo.cpp` | Collapsed map anchor and crosshair clamp coordinates |
| `Code/BF2/RendMisc/RenderView.cpp`, `Code/Tests/render_view_runtime.cpp` | Wider FOV cap and existing fixture expectations |
| `Code/BF2/Geom/TerrainDraw.cpp`, `Terrain.h` | Water-buffer capacity guard and cursor contract |
| `CMakeLists.txt`, `Code/Tests/syntaxcheck.cmd` | Client source manifest, explicit syntax gate and policy test target |
| `Code/Tests/hud_widescreen_policy.cpp` | Geometry/lifecycle fixture |
| `docs/building.md`, `docs/widescreen-hud.md` | Usage, attribution, contracts and acceptance scope |

## Validation

The content-free policy target exercises automatic vanilla selection, mod
gating, independent HUD lifetimes, resize/minimize handling and
golden canvas/edge/fullscreen geometry for 4:3, 5:4, 16:10, 16:9, 2560×1080 and 32:9.
Its System identities and callback storage are test scaffolding; it does not
exercise the renderer, builder scripts, event traversal or gameplay.

The existing MSVC x86 `Code/Tests/render_view_runtime.cmd` fixture checks adjusted
FOV, including 2560:1080, 32:9 and the unchanged narrow-screen behavior.

```sh
cmake --preset linux-x64 -B out/build/hud-review -DBUILD_TESTING=ON
cmake --build out/build/hud-review --target bf2_hud_widescreen_check
ctest --test-dir out/build/hud-review -R '^bf2_hud_widescreen_check$' --output-on-failure
```

Before submission, build the native client with its matching renderer and run a
visual/input pass with real vanilla content at the ratios above. Check chat,
tickets, ammo, squad/VOIP lists, warnings, minimap and expanded/spawn/commander
maps; test pointer access and clicking at both screen edges. Exercise maximum
weapon deviation, TV overlays, white-out, resolution switching, HUD reload and
mod restart. Compare vanilla behavior and an unsupported mod. If a local
server is needed, explicitly configure an unused port other than 16567.

Check the world camera at 16:9, 2560:1080 and 32:9, including zoomed views, and
exercise the water-grid path on a water-heavy map with a wide view. Check for
missing water at the buffer limit; passing CPU checks cannot establish visual
coverage or exercise the GPU submission path.

Linux policy checks and translation-unit compilation do not establish a working
native client, retail x86 ABI compatibility, or visual correctness. Record the
MSVC x86/x64 and native gameplay results separately before submitting a PR.

### Local draft validation

This uncommitted draft was prepared after pulling `main` at
`70641c25daddb6f2280e22cbf22f9894ec424493`.

- GNU C++14/Linux: policy target built and its CTest passed (761 assertions,
  including repeated-frame checks).
- An isolated `linux-x64` build of `bf2` and `dice_py_check` passed with strict
  links, including `libaidll.so` and `libdice_py.so`. This is dedicated-server
  build coverage; it does not compile the full client or exercise HUD rendering.
- Nine affected client translation units passed Linux syntax checks with
  `DEDICATED=0` and the real forced headers. `BF2Engine.cpp` and `HudAmmo.cpp`
  encounter Windows-only declarations on Linux; compiling the untouched baseline
  produced the same error messages. The modified builder was checked again after
  the final anchor-order adjustment.
- MinGW x64 syntax checks additionally passed `HudAmmo.cpp` and the policy/node
  files. It cannot replace MSVC validation: `BF2Engine.cpp` rejects the unsupported
  CRT/STL ABI, and `Ref2System.cpp` conflicts with the existing CRT compatibility
  definitions. No production compatibility guards were relaxed.
- The added `RenderView.cpp` and `TerrainDraw.cpp` changes passed GNU/Linux and
  MinGW x64 syntax checks with `DEDICATED=0`, `BF2_IMAGE_RENDDX9` and
  `BF2_STATIC_PLUGINS=1`. These use the existing graphics declarations and check
  both platform branches; they do not validate the MSVC plugin ABI. The existing
  x86 RenderView fixture was updated but not run: MSVC is unavailable here.
  The water guard has not been exercised at runtime.
- `git diff --check` passed. No native Windows/macOS client build, renderer run,
  actual HUD-script construction, click/event traversal, gameplay, or server
  runtime was exercised. No server port was opened. Existing named frontiers
  listed above remain in place.

Build and syntax logs are under the local `out/build/hud-review/` directory.
