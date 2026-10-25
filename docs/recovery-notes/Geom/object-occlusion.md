# Client object occlusion

The normal client uses `Geom/ObjectDrawer.cpp` in the native x86 RendDX9 image.
Its source manifest is `Code/Build/renddx9.cmd`. This recovery restores the
software occlusion pass between scene visibility collection and visual updates.
It changes no network messages, asset formats, factory registrations, virtual slots
or object layouts.

## Frame flow

1. ObjectManager collects static and dynamic collision objects using the frustum,
   per-object sphere tests and distance/fade hierarchy.
2. ObjectDrawer sorts those candidates, then builds occlusion volumes from nearby
   geometry with world-space occluder records. Each volume contains camera-facing
   source faces and planes through its silhouette edges.
3. Objects wholly inside any volume are removed in place. Precise mode lazily tests
   transformed geometry box corners when a sphere intersects a plane. Without
   geometry, the conservative sphere test applies.
4. The noncollision draw set is tested against the same volumes. The existing
   overgrowth and undergrowth queries also consume that plane list.
5. Each enabled update clears the preceding frame's volumes before rebuilding.
   Destruction unregisters the event listener and releases plane lists, object-set
   storage, the retained occluder and both tree containers.

An object touching a plane can be hidden; one extending outside stays visible.
The object test uses its absolute position and visual radius, and transforms local
geometry bounds with the object's absolute transformation. It does not substitute
the geometry's stored transformation or a recomputed box center. Mode selection
occurs once per cull pass. Candidate order and the geometry-only rejection counter
are preserved.

## Original evidence

Inspected directly through IDA, using the BF2 1.50 x86 PE
`RendDX9.dll`, MD5 `aaf43741c3e1504f8d1498d366f9f4fa`.
The IDB catalog names it `RendDX9.dll (2).i64`; the similarly named entry without
`(2)` is a different game's renderer and was not used as recovery evidence.
The original source string at `1021AF98` identifies
`BF2Branches/Patch_1_50/Code/BF2/Geom/ObjectDrawer.cpp`.

| Original location | Contract inspected |
| --- | --- |
| `100F9790`, especially `100F9833/100F983B` | Build volumes, then cull the collision set before visual updates |
| `100F9083` | Reuse volumes to cull the noncollision draw set |
| `100F9300–100F978C` | Distance and facing gates, shared edge flags, silhouette construction, nested plane-list append |
| `100777A0` | Normal normalization and near-zero/unit-length handling |
| `100F8430–100F8745` | Sphere/box rejection, lazy box construction, ordered erasure and counter |
| `100FAFF0`, `100F9210` | Reference release and destruction order |
| `100F9A40/100FA230`, `100ED1C0` | Tree-node and nested-vector cleanup |
| `100FAB70/100FAA10/100FA670` | Copy the temporary plane list before destroying it |

The scene-side checks were also read in the original BF2 client PE, MD5
`4c843dcee79388341f72c3a7adb1ace6`: ObjectManager wrappers
`7267E0/726810` call `7255D0`; BinTree and QuadTree frustum entry points
`72CBD0/72CAB0` and `72B1A0/72AF40` contain real rejection/acceptance walks.
ObjectDrawer's predicate mask is `2`, so its normal queries do not select the
`0x01000000` distance-filter bypass.

RendDX9 is a Windows client image. The symbolized dedicated Linux image supplies
no implementation of this renderer pass, so no ELF parity claim is made.

## Storage and retained behavior

The existing geometry record contract remains: a 36-byte instance record contains
two fixed-layout vector headers and an edge-vector pointer. World-space faces are
20 bytes (plane plus template edge-reference pointer); points are 12 bytes; edges
are four bytes, and face-to-edge references are two bytes. Existing StaticMesh
loading/transformation code produces these records.

The original builder takes each face's reference count from the **first face**,
while reading references from the current face. This observed quirk is retained;
it is not generalized into a new malformed-asset policy. Distance equality is
accepted, facing-angle equality is rejected, and the observed unordered-comparison
behavior is retained. Dot products are not rounded to float before the cull
comparison; native arithmetic is not claimed bit-identical to x87 in every case.

The existing `vc71` allocation adapter still owns fixed-layout buffers and retains
its documented `RW-74` capacity-policy limitation. The temporary plane list is
copied before destruction, and scoped ownership releases buffers if allocation
fails. Object sets use native STL operations. No retail-plugin compatibility is
reintroduced.

## Validation and remaining work

The focused regression is `Code/Tests/object_drawer_occlusion_runtime.cmd`.
It is an MSVC x86, C++14, native-plugin, `DEDICATED=0` executable with a strict link
(no `/FORCE`). It includes the actual drawer implementation and narrow test-only
object, geometry, view and event collaborators; unsupported fixture calls abort.

- Production drawer fixture: **111/111 checks passed**, strict link. This includes
  the actual `updateVisible` and `draw` calls, hidden/exposed/tangent/straddling
  objects, missing geometry, both precision modes, translated cameras, both edge
  windings, multiple volumes, repeated frames, survivor order, and teardown
  ordering/allocation balance. A test-only baseline using the previous source
  fails five public-path checks and emits the old skipped-occlusion diagnostics.
- Existing stock occluder-loader fixture: **95 files, zero failures and zero edge
  errors**. That older fixture uses `/FORCE:UNRESOLVED`; it validates the loader's
  records, not the complete rendering path.
- Isolated native renderer: **126/126 translation units compiled**, strict DLL link
  and native ABI/import checks passed. The changed translation units and link
  produced no warnings. This is renderer build validation, not a full client
  executable link or gameplay run.

The changed files are `Geom/ObjectDrawer.cpp` (builder, cull and cleanup),
`Geom/ObjectDrawer.h` (nonvirtual destructor declaration and storage contract),
`Geom/ModuleGeom.cpp` (retired fallback comment), the two focused test files,
and this note. The renderer-private header is included by ObjectDrawer.cpp and
RendDX9/BF2Render.cpp; it does not enter the dedicated or client executable images.

Reproduce the focused check from the repository root:

```powershell
$env:BF2_BUILD_OUT = "$PWD\Code\Build\obj_occlusion_check"
cmd /c Code\Tests\object_drawer_occlusion_runtime.cmd
Remove-Item Env:BF2_BUILD_OUT
```

The local validation artifacts are under
`Code/Build/obj_culling_recovery_tests/production`,
`Code/Build/obj_culling_stock_occluders`, and
`Code/Build/obj_culling_renderer`. The separately built native DLL is
`Code/Build/bin_culling_renderer/RendDX9.dll`.
The validated DLL and matching PDB were also staged beside the normal client in
`cmake-build-debug/bin_client`; both hashes match the isolated build. Reference
game files were not modified.

The `RW-objectdrawer-occluders` report-and-continue fallback is removed; the
covered builder and cull path are enabled. Other ObjectDrawer boundaries remain:
the optional `+0x88` pass (`100F8750`), slots `+0x1C/+0x20`, the unresolved
ECCore event payload, and the separate first-frame census marker. This change does
not claim to complete those paths.

No live gameplay or before/after frame-time measurement is established by these
tests. Existing renderer traces perform synchronous logging, normal Debug engine
objects are unoptimized, and renderer wall time includes GPU waits. To attribute
the remaining slowdown, compare stationary sparse/dense camera headings with the
same map, view position, resolution, settings and build, and inspect CPU/GPU samples.
Do not enable frontier-backed profiling overlays as though they were complete.
Any agent-run server session must explicitly use an unused port other than 16567.

## Native culling-debug gate

The newly active culling pass exposed a stale retail-offset read in
`RendDX9/BF2Render.cpp`: renderFrame tested byte `0x70` of ObjectDrawer to enable
the unfinished `Culled:` overlay. With the two native twelve-byte object vectors,
the actual debug flag is at `0x68`; `0x70` is the noncollision culled-object
counter. Removing an object therefore falsely selected the overlay frontier.

Directly inspected original PE instructions at `10030A66–10030A74` load
ObjectDrawer and test its retail debug byte at `+0x70` before calling `10029BB0`.
The constructor at `100F9130` clears EBX at `100F9153` and stores BL into that
byte at `100F91D0`. These are the same verified renderer image identified above.
The correction uses a named, nonvirtual `isCullingDebugEnabled()` query of that
flag, preserving the existing null guard. The query adds no storage or vtable
slot and changes no ABI, packet or file format.

The overlay body remains a named frontier when its actual flag is enabled;
this correction does not implement or suppress the optional overlay. The source
audit found no production writer of that flag beyond constructor initialization.
Other ObjectDrawer consumers use its existing virtual query slots and have no
matching raw member-offset read.

The isolated build under `Code/Build/obj_culled_overlay_renderer` passed all
126 translation units, strict linking and native ABI/import checks. Its compiled
BF2Render code calls the flag query, whose instructions read native byte `+0x68`;
the enabled branch still reaches the frontier. The existing `RaShader`
class/struct warning in BF2Render also occurs in the preceding builds; the
ObjectDrawer compilation and link were clean.

The extended strict occlusion fixture passed **128/128 checks** under
`Code/Build/obj_culled_overlay_tests`. After actual `draw()` culls one object,
the legacy byte read is nonzero while the production flag query remains false.
Additional cases verify counts 0, 1, 255, 256 and 257 cannot select the overlay,
and explicitly setting and clearing its flag controls the query independently.
This is a component regression plus inspection of the compiled renderer gate;
it does not run the full renderFrame loop or a joined game session.

The corrected DLL and PDB from `Code/Build/bin_culled_overlay_renderer` were
staged beside the normal client in `cmake-build-debug/bin_client`; both hashes
match. The DLL SHA-256 is
`1568d8d5ef811255afd344b591503f8b539dd814711b1b8054f996627713e892`.
The source changes for this gate are `RendDX9/BF2Render.cpp`,
`Geom/ObjectDrawer.h`, the existing occlusion fixture and this note.
