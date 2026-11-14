# c-raw-slots: raw vtable-slot dispatch across the native image boundary

[CHOSEN, not recovered] This is macOS port work. MSVC paths are unchanged: every edit is a
non-MSVC arm, or wraps the original MSVC code in `#if defined(_MSC_VER)` without changing its tokens.

## The bug

The loopback join ([harness](macos-c-input-script.md#harness-overview)) crashed in `GameLogic::load`, in the chain
`ArmorGLComp::init` → `geometryGetGeomCount` → `StaticMesh::setLod`. On macOS the geometry
instances come from the static renderer image. Its tables are explicit **PE-order** function
tables (Geom/NativeGeometrySlots.h, a1). Game code's non-MSVC branches indexed them with **ELF**
offsets. For example, getGeomCount ELF +0x78 is ordinal 30, which is `setLod` in PE order; the
right entry is ordinal 29, `getGeomCount`. This is a1's "raw geometry selector" class, in game
code that a1 did not cover.

## Rule applied (coordinator decision)

On macOS, a cross-image call goes through a real native interface declaration (a typed virtual
call) or a named adapter that the audit checks. PE and ELF offsets stay only as evidence in
comments and in the MSVC/ELF arms.

## Changes

- **Geometry instance operations.** `Game/Objects/GeometryPartAccess.h` gains 22 named operations:
  - `setFov`, `getFov`, `getBoundingRadius`, `setGeometryTransform`, `setBoneTransforms`,
    `gatherUsedBones`, `setExcludedPart`
  - `setSubgeom`, `getSubgeom`, `getSubgeomCount`, `setUvTranslation`, `setUvTransformation`
  - `setOwnerObject`, `updateFlags`, `setNextGeometry`, `getNextGeometry`, `setFirstPerson`
  - `setLightSourceMask`, `getLightSourceMask`
  - plus the existing part/LOD/template ones.

  Static native builds select the named ordinal from `Geom/NativeGeometrySlots.h`. MSVC keeps the
  PE offset, and other non-MSVC builds keep the ELF offset the call sites documented. The
  converted callers are ArmorGLComp, Kit, Camera, Bundle, SimpleObject, AntennaObject,
  AnimatedBundle, DestroyableWindowsBundle, Rotor, Soldier, SoldierAnimation, MaskObject,
  TargetComp, GenericFireArm, SingleFireComp, DefaultZoomComp, GenericProjectile, Object and
  ObjectManager. The raw helpers that remain (`vslot`, `bundleVslot`, `simpleVslot`,
  `objectRawSlot`) are now MSVC-only, so a forgotten native caller fails to compile.
- **SoldierAnimation +0x60, settled from the binary.** The game called it as
  `handleMessage(TemplateMessage, void*)`. RendDX9 SkinnedMesh +0x60 (0x101A67E0) reads
  `cmp byte [ebp+0xC],0`, then stores `part` or -1 at +0x17C. So it is
  `setExcludedPart(int, byte flag)`, and the native call passes the payload's low byte as the flag.
- **Geometry templates.** `SimpleObject::createGeometry`, `SimpleObjectTemplate::getAnchorPos`
  and `lookupTracerGeometryTemplate` now use the typed `IGeometryTemplateManager::getTemplate` on
  `!DEDICATED && BF2_STATIC_PLUGINS`. This is the a1-audited `geometry-registry` view; raw
  [0x18/4] was correct only by coincidence.
- **ClassManager getSingletonAs (renderer → host).** This was a real bug.
  RoadEditable, UndergrowthSystemLoad and OverGrowth called `[0x4C/4]`, which on the native
  Itanium host table is ordinal 19, `getSingleton`, instead of 20, `getSingletonAs`. `HfeAbi.h`
  now types `getSingletonAs(const HostString&, unsigned)` natively; MSVC keeps
  `reserved_getSingletonAs`. The `class-manager` pair still passes (27 slots).

## Gate

- `tools/macos/raw_slot_scan.py` runs inside `bf2_vtable_audit` as pair `raw-slot-dispatch`.
  - It finds every function-table load from an object (`*reinterpret_cast<void***>(x)`,
    `(*(void***)x)`) in non-MSVC-reachable code under `Code/BF2`, External excluded. MSVC-only
    arms are evaluated, not guessed.
  - It compares them with the reviewed `tools/macos/raw_slot_baseline.tsv`: path, normalized
    code and a classification per row.
  - A new site fails the gate, and so does a baseline row whose site disappeared.
  - Negative test: one added raw dispatch fails it with "new raw slot dispatch in non-MSVC code".
- The `geometry-selectors` check now pins 35 role/provider pairs across the BundledMesh,
  StaticMesh, SkinnedMesh and MeshParticleMesh tables (it used to check 8 Bundled roles).

## Inventory: 158 baseline sites

| Group | Sites | Status |
| --- | ---: | --- |
| Game/Scene/Physics/Menu/Core | 20 | classified per row |
| Renderer image (Geom, GeomMisc, RendDX9, RendMisc) | 138 | inventoried, **not individually reviewed** |

The 20 game-side rows:
- **Correct on macOS:** post-production framework ordinals (a1 native_post), the EnvMap
  manager/template (real C++ providers, declaration order without a destructor in front), and
  the geometry adapter itself.
- **Not reachable on macOS:**
  - non-macOS arms of the template-manager lookup;
  - dedicated null-receiver reproductions;
  - `IResourcePriority`, which no native provider answers.
- **Debug-only, provider unverified:** DebugCounterManager, Zipline/SoldierResponse debug
  drawing, CollisionMeshTemplate debug meshes.

Renderer spot-checks:
- The ClassManager sites were wrong; now fixed.
- `BF2Render` fog calls use PE ordinals 46–51 into the engine `ObjectManager`. They precede its
  virtual destructor (+0x11C), so they are correct by declaration order, but still raw.
- `StaticMesh` +0xF0/+0xF4 on the object template is unreviewed.

**Open:** review the 138 renderer rows. Those that reach engine (BF2-image) objects are the same
bug class in reverse; replace them with typed calls.

## Validation

- `bf2_client` (renderer ON) and `bf2` build. `bf2_vtable_audit`: 135 pairs, 0 failures
  (`raw-slot-dispatch` 158, `geometry-selectors` 35, `class-manager` 27).
- **Loopback** (`--build c-raw-slots --client-seconds 120`):
  - The `GameLogic::load` crash is gone.
  - The client loaded `dalian_plant` (shaders, templates, particle systems) and ran 1,900+
    update/display frames for 120 s without a crash or a new frontier.
  - The server still showed `Players: 0/64 (1 connecting)` at the end. The join does not
    complete; the client stays on the loading screen.
  - Where the loading thread waits is not identified yet: a late lldb break could not run,
    because another agent's server held UDP 16890.
