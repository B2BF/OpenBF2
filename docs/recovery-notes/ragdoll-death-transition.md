# Ragdoll pose at death

The reported case is an ordinary rifle kill viewed by a retail client connected
to the OpenBF2 dedicated server. The corpse rises and flails before falling.

## Recovered correction

`Soldier::enableRagDoll` in `Code/BF2/Game/Objects/Game/SoldierAnimation.cpp`
evaluates the collision skeleton before passing it to
`RagDollTemplate::makeInstance`. Retail evaluates that skeleton with an identity
parent matrix whose local Y translation is **-1 metre**. The reconstructed body
passed a pure identity matrix, leaving a standing soldier's initial ragdoll
particles one metre too high.

The correction restores `skeletonOffset.m[13] = -1.0f` before the existing
`Skeleton::transform` call. `RagDoll::reset` seeds current and previous particle
positions from that pose, preserving the inherited velocity difference. Existing
replication sends those particle positions to the retail client; an elevated
server pose can therefore produce an upward correction on that client.

This is a recovery correction, not a new ragdoll tuning policy. Velocity selection,
death forces, update ordering, class layout and network encoding are unchanged.
The code mismatch is established; visual confirmation of the complete reported
symptom still requires a joined-player retest.

## Original evidence

Fresh inspection used the original 32-bit BF2 1.50 dedicated images:

| Image | MD5 | Evidence |
| --- | --- | --- |
| Windows PE | `3ca127b1be4275196af93347714707f9` | `enableRagDoll` at `0x5BEAE0`: matrix base `ebp-0xD8`; `0x5BEC7F` stores `0xBF800000` at `ebp-0xA4` (element 13). `Skeleton::transform` follows at `0x5BEC93`, and `makeInstance` at `0x5BECB4`. |
| Linux ELF | `aa94014ed5d4308fb86655f5c890b59f` | `enableRagDoll` at `0x81B66D0`: matrix base `ebp-0x148`; `0x81B68B2` stores `0xBF800000` at `ebp-0x114` (element 13). `Skeleton::transform` follows at `0x81B6946`, and `makeInstance` at `0x81B6976`. |

Both instructions store IEEE-754 `-1.0f`. The surrounding PE velocity selection
and `triggerDieAnimation` force routing agree with the existing source in the
examined paths. No additional event/network defect was established by the scoped
source audit.

## Validation

The shared `SoldierAnimation.cpp` source belongs to the dedicated and client engine
manifests. MSVC x86 C++14 syntax/semantic checks passed with both `DEDICATED=1` and
`DEDICATED=0`. The Windows x64 dedicated strict build passed; the Windows x86
Reconstruction compile and diagnostic `/FORCE` link passed. The Linux x64
dedicated strict rebuild also passed.

`Code/Tests/ragdoll_transition_runtime.cpp` and its Python runner strict-link the
actual engine objects and call the real Soldier, Skeleton, RagDollTemplate and
RagDoll implementations. A static fixture pose disables animation playback via
cvars. Two cases cover translation, yaw, stationary/moving soldiers, all thirteen
fixture particles, initial hip position, velocity history, physics disabling and
repeated enable calls. The fixture also passes MSVC x86 C++14 syntax checking.

- Fixed code: **68/68 checks pass**.
- Negative control using the preserved old `SoldierAnimation.cpp.o`: **30 expected
  position failures**. Hip positions are `(7,11,-11)` instead of `(7,10,-11)`, and
  `(-17,5,9)` instead of `(-17,4,9)`. The fixed code produces the expected positions.
- All 26 inherited-velocity checks and both repeated-call checks pass in both
  versions. The correction moves current/history positions equally.

Run against an existing strict Linux dedicated build:

```sh
python3 Code/Tests/ragdoll_transition_runtime.py \
  --engine-build ~/bf2-linux-local --output ~/bf2-ragdoll-check
```

The runner's optional `--animation-object` supplies a previously preserved object
for the negative control without modifying the engine build. It does not suppress
unresolved symbols or replace the production transition with a test body.

Local build logs are under `out/ragdoll-transition-20261004/`; positive/negative
runtime logs are in its `fixture/` subdirectory. No server or client was launched,
and damage events, physics ticks, collisions and rendered death animation were not
exercised. No frontier was removed by this correction. The
ragdoll debug-draw and constraint-export frontiers remain outside the ordinary
death transition and this change.
