# Projectile detonation position

`GenericProjectile::detonate` must retain the reference returned by
`getAbsolutePosition()` while it relocates a collided projectile to the recorded
contact point. Copying the position before that relocation sends an obsolete blast
center to the detonation component. This is a fidelity repair, not a new damage rule.

The report concerned the Black Hawk side miniguns. Stock
`Weapons/stationary/HMG_M134/HMG_M134.tweak` in
`D:\B2BF Launcher\Battlefield2\mods\bf2\Objects_server.zip` configures
`HMG_M134_Projectile` with explosion material 40, radius 1.5, damage 200 and force 0.
The pre-fix dedicated content-loading probe confirmed those values; it proves the
existing bindings loaded correctly, not post-fix gameplay. The small radius
makes a displaced blast center consequential; zero explosion force does not disable
the damage branch. This repair does not change any of those authored settings.

## Original instructions

Both original Battlefield 2 1.50 dedicated images were inspected directly in
read-only IDA workers: the symbolized 32-bit Linux `bf2` ELF and Win32
`bf2_w32ded.exe` PE.

| Operation | Linux ELF | Windows PE |
| --- | --- | --- |
| Detonation function | `0x8282E00` | `0x5B4400` |
| Obtain and retain position pointer | call `0x8282F15`, save `0x8282F18` | call `0x5B44C4`, save EBX `0x5B44CF` |
| Relocate to stored collision point | `0x8282F6D` | `0x5B4518` |
| Read XYZ through retained pointer | `0x8282FB6–0x8282FC6` | `0x5B45C4–0x5B45D7` |
| Invoke detonation component | `0x8282FFE` | `0x5B45E7` |

The getter returns the inline absolute transform translation: ELF
`Object::getAbsolutePosition` at `0x830B440` returns `this + 0xB8` at
`0x830B463`; PE `0x6DC0C0` does so at `0x6DC0F4`. The root-object arm of
`setAbsolutePosition` overwrites that same XYZ storage: ELF `0x830AFA0`, writes
`0x830AFCA–0x830AFD3`; PE `0x6DCB10`, writes `0x6DCB27–0x6DCB33`.
The current typed `Object` methods preserve this alias relationship on native x64.

The production change is solely `const Vec3` to `const Vec3&`, plus its local
contract comment in `GenericProjectile.cpp`. It preserves call order, collision
stamp checks, arming, ownership, component dispatch, hiding and destruction.
No interface, layout, packet, material or file-format contract changes. There is
deliberately no second position getter: for parented objects the setter dirties the
absolute cache after updating the relative transform, and the original keeps its
existing reference rather than refreshing that cache here.

## Regression and build validation

`Code/Tests/projectile_detonation_position_runtime.cpp` links the complete actual
engine, including the production `GenericProjectile.cpp` and `Object.cpp` objects.
It constructs real projectile/templates and records contact through actual
`handleCollision`. A test detonation component observes the by-value arguments;
an object-manager observer checks destruction messages. Cold abstract game-policy
callbacks abort. No production implementation is replaced by a modeled body.

Run inside the configured Linux environment:

```sh
python3 Code/Tests/projectile_detonation_position_runtime.py \
  --engine-build ~/bf2-linux-bundle-nullcall/build \
  --output ~/bf2-detonation-position-test
```

For a negative control, pass `--component-object` with a preserved pre-fix
`GenericProjectile.cpp.o`. The runner strict-links in a separate directory and
renames `main` only in its private copy of the original main object.

- Fixed object: **44/44 checks passed**.
- Saved pre-fix object: **40/44**, four intended failures across two displaced
  collisions. The old object relocated correctly but passed `(10,20,30)` instead
  of `(10,0,30)`, and `(-12,3,17)` instead of `(4,-5,-6)` to the component.
- Stationary collision and non-collision sentinel cases passed in both versions.
- Independent downstream diagnostic: **109/109 checks**, using real
  `GameLogicExplosion::serverHandleExplosionOnObject` and damage queue draining.
  At radius 1.5 with damage 200 and a test material coefficient of 1, distances
  0, 0.5, 1 and 1.49 queued damage; 1.5, 1.51 and 10 did not. Child-to-root armor
  and collision-surface distance behavior were also covered.
- MSVC x86 real-TU syntax checks passed for `DEDICATED=1` and `DEDICATED=0`.
  The Windows x64 dedicated strict link, Linux x64 `bf2` strict link and
  `dice_py_check_run` passed.

Local logs are under `out/splash-fix-20261004/`; the downstream diagnostic lives
under `out/splash-damage-20261003/`. The preserved pre-fix source/object and native
fixture outputs are in `/home/user/bf2-projectile-detonation-20261003/`.

These tests prove the repaired center argument and its effect on the existing
radius calculation. They do not exercise joined minigun firing, visibility/grid
selection, final health/score mutation, client effects, network delivery or map reload.
The parented-cache behavior is preserved by the source change but was not exercised
by this fixture. Existing `fx-explosion-diagnosed` tracing and the
`bfcollisionhandler-iface` AI collision frontier remain unchanged.
