# Sticky projectile attachment

C4 and claymores were reported to land, then jump or slide before settling when
a retail client joined the rebuilt server. The attachment audit found three
differences from the original dedicated engine: an x86-only network-ID read in
the event producer, a parent check in place of the destruction check before
applying a pending attachment, and a water test using the projectile material
instead of the struck surface's material.

The repair changes only
`Code/BF2/Game/GameEvents/StickyProjectileEvent.cpp` and
`Code/BF2/Game/Objects/Projectiles/CollisionComponents/StickyCollisionComp.cpp`.
These are recovered behavior and native-layout corrections, not new attachment
policy. No header, virtual declaration, object layout or packet format changes.

## Original image evidence

The Battlefield 2 1.50 dedicated images were inspected directly in read-only
IDA sessions, including the relevant instructions, tables and callers.

| Image | Architecture | Input MD5 |
| --- | --- | --- |
| `bf2` | ELF, x86 32-bit | `aa94014ed5d4308fb86655f5c890b59f` |
| `bf2_w32ded.exe` | PE, x86 32-bit | `3ca127b1be4275196af93347714707f9` |

### Attachment event identifies the projectile by its network ID

`StickyProjectileEvent` obtains the projectile's networkable and reads its
16-bit ID. The ELF complete-object constructor is `0x807C4C0`, with the ID load
at `0x807C572`; its base-object constructor is `0x807C220`. The call sites include
`ServerConnection::clientSendDatabase` at `0x808A35D` and the pending-stick update
at `0x828BCC8`.

In PE constructor `0x53E0A0`, `0x53E128` calls the object's primary `+0xD4`
virtual slot, `getNetworkable`. Instruction `0x53E12E` loads a word from the
returned networkable at `+4`, and `0x53E132` stores it in the event's projectile
ID field. The target path independently uses the same contract: the call at
`0x53E182`, word load at `0x53E188`, and event store at `0x53E195`.
The projectile's actual primary table resolves `getNetworkable` to `0x546670`,
which reads the object's networkable pointer at `+0x4C`.

The old source copied the original networkable `+4` byte offset into a raw
pointer read. That offset follows a four-byte virtual-table pointer in the
original images. In the native 64-bit class, the virtual-table pointer occupies
eight bytes and `m_networkableId` follows at `+8`; reading `+4` instead reads part
of that pointer. The producer now uses
`projectile->getNetworkable()->m_networkableId`, as the target-ID helper already
does. This retains the original single dispatch and its null assumptions while
letting the compiler use the current layout.

An incorrect projectile ID can prevent a receiving client from applying the
attachment to the intended projectile. The current client consumer returns for
ID zero or a failed network-object lookup. This is a concrete producer defect
and a plausible explanation for a disagreement between predicted and
authoritative landing. A joined retail-client run has not yet established that
it explains every reported jump or slide.

### Pending attachment checks destruction, not parenting

`StickyCollisionComp::updateComponent` consumes a pending attachment only when
the component has its pending flag, the process is the server or predicts this
projectile locally, and the projectile is not awaiting destruction. It then
clears the pending flag, applies the attachment, and sends the event unless the
projectile is locally predicted.

The ELF body is `0x828BC00`. Its virtual call uses `+0x44` from the primary
address point of table `0x88BC340` (the address point is table start plus eight).
That slot resolves to `Object::getObjectInDestroyList` at `0x8116600`, a Boolean
read of the original object's `+0x12A` field.

The PE body is `0x5B6070`. Its call at `0x5B60B1` uses primary slot `+0x40`,
then tests `AL` and returns when it is nonzero. `GenericProjectile` constructor
`0x5B4670` installs primary table `0x7DB6B0` at `0x5B469E`. That table's
`+0x40` entry is `0x5466B0`, a byte read of object field `+0x142`; adjacent
setter `0x5466A0` writes the same field. Original
`ObjectManager::addObjectToDestroyList` (`0x6E0D50`) invokes that setter with
true at `0x6E0D79`–`0x6E0D7F`.

The previous source checked `getParent()` instead. The repaired source uses
the existing typed `getObjectInDestroyList()` method. Parenting checks elsewhere
remain: PE `handleCollision` reads the parent directly from `+0x28`, while the
attachment methods remove an existing parent when their respective original
conditions require it. This correction neither adds a new destruction guard nor
makes parenting itself a reason to discard an already pending attachment.

### Water rejection uses the contacted material

`handleCollision` receives the projectile material, the target material and an
optional override for the projectile material. It forwards the selected
projectile material and the unchanged target material to game collision logic.
Later, the water rejection must query the target material.

In ELF body `0x828BEB0`, instruction `0x828C5DE` loads the second integer
argument (`arg_18`) for the material lookup through `+0x24` at `0x828C5EA`.
In PE body `0x5B6200`, `0x5B626E` selects the override from `[ebp+0x20]`,
falling back to the projectile material at `[ebp+0x18]` at `0x5B6276`.
The later lookup instead loads `[ebp+0x1C]`, the target material, at
`0x5B6520` and calls material-manager slot `+0x28` at `0x5B6526`.

The repaired source names that existing second integer `targetMaterial` and
uses it for the final lookup. The argument type and order do not change. Null
material and water still refuse attachment, followed by the existing vehicle,
terrain and object permission tests.

## Preserved behavior and compatibility

The immediate orientation adjustment, deferred attachment, weak host ownership,
nearest rotational-bundle selection, push-out calculation and physics disable
sequence remain unchanged. PE `stick` (`0x5B6140`) and `stickAbsolute`
(`0x5B5BC0`) confirm the existing sequence: apply the transform, enable separate
physics update, disable the physics node and set the component's disable latch.
Reset continues to restore physics through its existing path.

Event type 32 and serialization are unchanged: projectile and target IDs remain
16 bits, target traversal count and child index remain conditional eight-bit
fields, and player ID remains eight bits. The twelve transform floats retain
row order 2, 0, 1, 3. The change corrects the value supplied to the existing
protocol; it does not introduce a native-size value on the wire.

The pre-existing `[UNRESOLVED]` word in `StickyCollisionComp.h` remains untouched.
The client `findChildTypeIndex` call still carries its `[FRONTIER]` annotation
even though the helper has a source body. That annotation and the event header's
stale producer/client-frontier comment are outside this repair. No frontier is
retired and no broader client attachment completeness is claimed here.

## Validation status

Build checks passed: MSVC x86 C++14 permissive syntax for both changed translation
units with `DEDICATED=0` and `DEDICATED=1`, Windows x86 dedicated compilation and
diagnostic `/FORCE` link, and Windows x64/Linux x64 dedicated strict links. Each
engine compile selected its image and force-included `Core/BuildConfig.h`.

The focused regression is `Code/Tests/sticky_projectile_runtime.cpp`, driven by
`Code/Tests/sticky_projectile_runtime.py` against an existing Linux CMake build.
It strictly links the production engine objects, renaming only the application
entry point in a private copy. MSVC x86 fixture syntax also passed.

| Linux regression configuration | Checks | Failures |
| --- | ---: | ---: |
| Corrected event and collision objects | 105 | 0 |
| Preserved old event object, corrected collision object | 105 | 14 |
| Corrected event object, preserved old collision object | 105 | 10 |

The cases cover IDs zero, `0x1234` and `0xFFFF`, absolute and target-relative
attachments, independent wire-field and matrix-row decoding, truncated packet
rejection, differing projectile/target water flags, and pending attachment with
parenting and destruction varied independently. Event execution resolves the
projectile through the real network manager and applies the real sticky delegate.
Moving the projectile before execution checks that attachment restores the
received transform rather than merely observing its initial position.

The fixture's collision policy accepts contacts and its projectile activation
counts calls without starting effects. It does not exercise physics-node freezing,
the local-owner weapon tail, a multi-level rotational target tree or network
transport. The saved-object controls run to completion and demonstrate that the
assertions detect the previous implementations independently.

Run after building the dedicated engine, with fixture output outside that build:

```sh
python3 Code/Tests/sticky_projectile_runtime.py \
  --engine-build "$HOME/bf2-linux-local" \
  --output "$HOME/bf2-sticky-check"
```

For a negative control, add `--event-object` or `--collision-object` with the
corresponding object preserved before rebuilding. The runner leaves the engine
objects and executable untouched.

No map, socket listener or client/server session was started for this validation.
Retail-client testing still needs C4 and claymore placement on terrain, static
objects and moving vehicles, including the first landing and subsequent stable
attachment. Build and fixture success alone cannot establish that the reported
visible motion is resolved.
