# Vehicle input and guided-target audit

The reported configuration was a retail client connected to the rebuilt Linux
x86-64 dedicated server. Reports covered delayed aiming toward vehicles,
choppy helicopter/TV-missile control, and a tentative difference in jet
missile targeting. The aiming report was subsequently narrowed to a jet's
mounted machine gun: after rapidly turning toward a vehicle and firing,
damage appears late even though the jet itself flies normally. These symptoms
do not establish one shared cause.

## Latency compensation

The seven reported variables are registered with the retail defaults:

| Variable suffix after `phy-latencyComp-` | Default |
| --- | --- |
| `default` | 0.05 seconds |
| `max` | 0.3 seconds |
| `maxRenderAheadTime` | 0.033333335 seconds |
| `pingFactor` | 0.5 |
| `useClientWindow` | 1 |
| `useInput` | 1 |
| `useRenderAhead` | 0 |

`debug` defaults to -1 and the initial enabled flag is true. The remote-player
calculation is half the ping plus the interpolation delay sent by that client,
capped at 0.3 seconds. With `useClientWindow=1`, the 0.05-second default does
not additionally enter that calculation. Input sampling and render-ahead
settings apply to local players; the connected retail client supplies its
interpolation/input delay in its player-action block.

Fresh inspection of the original ELF32 initializer at `835C6B0` and
`LatencyCompensation::getLatency` at `835C5B0` confirms those defaults and
that calculation. The dedicated PE32 implementation at `69E680` agrees;
its local helper is at `69E610` and conversion helper at `69E5D0`. Some PE
database names at these addresses were stale; the investigation followed
the variable references and instructions.

The current server has the surrounding integration: `Object::init` allocates
history for mobile, networked root objects; `PhysicsNodeManager::postFrameUpdate`
records it; projectile collision paths query compensated transforms.
The vehicle reader uses the original 16-sample ring and 30 Hz interval
(ELF `83610E0`). This audit does not justify changing the latency defaults
or claiming that compensation is globally absent.

A production-path Linux run confirmed the actual values in this table,
`debug=-1` and enabled=true after loading stock Karkand content. A physics
node allocated both history arrays and advanced the same ring through
indices 0 to 7. No connected player's compensation amount or projectile
collision was observed during that empty-server check.

Fresh ELF input and control-state inspection also found no changed input
index forwarding: `PlayerActionManager::processReceivedPacket` (`809DA20`)
decodes consecutive indices, `processRcvdPlayerActions` (`809D980`) queues
newer actions, and `GameServer::simulatePlayerUpdate` (`80B4E40`) consumes or
replays them. `GhostManager::updatePlayerControlObject` (`8096210`) captures
the acknowledged action index; `writeControlObjectState` (`80963E0`) sends it
separately from the world tick written by `transmit` (`8097FF0`). These checks
establish the inspected code paths, not timing under a live client's traffic.

For the mounted-gun report, fresh ELF instructions for
`FireComp::getProjectileOrigin` (`82647B0`), `launchProjectile` (`8263220`),
and `SingleFireComp::fire` (`826C400`) match the current launch-transform
selection and configured-delay branches. The ordinary mounted branch reads
the firearm's current absolute transform; launch samples the origin, applies
muzzle velocity and adds the root's physical velocity. The inspected point
collision routine (`8363DD0`) also uses the projectile owner's latency when
requesting the target's historical transform. No change to those paths was
justified by this inspection.

## Confirmed guided-target defect

`LPTargetComp.cpp` still selected literal 32-bit field offsets for three
accesses. Its fallback branch also applied those offsets to native x64
objects, although their pointer and container layouts differ:

| Access | Original ELF32 | Original PE32 | Current GCC x64 |
| --- | --- | --- | --- |
| Target template type | `+17C` | `+22C` | `+29C` |
| Target template lifetime | `+180` | `+230` | `+2A0` |
| Target owner/player ID | `+14C` | `+164` | `+264` |

The wrong native type read reached an animated-UV index instead of the
target type, so unique targets could take the networked creation branch.
The lifetime read reached UV flags/padding. The owner write reached unrelated
object padding and left the actual target player ID unchanged. These defects
affect wire/TV/laser targeting setup; laser deactivation also reads lifetime.
TV and wire systems already bypass that lifetime branch.

The correction removes the three raw-offset helpers. It reads the existing
template fields through `TargetObjectTemplate`, with narrow friendship for
`LPTargetComp`, and calls the existing `TargetObject::setTargetPlayerId`.
There is no new virtual method, object-layout change or wire-format change.

Both original BF2 1.50 dedicated images were opened read-only in IDA and
verified as x86/32-bit:

- ELF `ia-32`: MD5 `aa94014ed5d4308fb86655f5c890b59f`.
- PE `bf2_w32ded.exe`: MD5 `3ca127b1be4275196af93347714707f9`.

| Original behavior | ELF32 evidence | PE32 evidence |
| --- | --- | --- |
| Type 3 creates and initializes a local target; other types use `createObjectOnAllClients` | `initComponent` `8273170`, compare `82731DC`, local branch `827335D`, network call `8273249` | `initComponent` `5AC100`, compare `5AC157` |
| Activate first, then fetch weapon player and store that player's ID | `enableTargetObject` `8273600`, activation `8273634`, player/ID calls `8273641/827364C`, store `827364F` | `enableTargetObject` `5AC4B0`, activation `5AC4C8`, store `5AC4E2` |
| Skip target systems 3/4 and absent targets; otherwise post message 30 using template lifetime | `fireStop` `82736B0`, lifetime `82736E1`, post `8273707` | `fireStop` `5AC520`, lifetime `5AC548`, post `5AC56F` |

The changes preserve those guards, call order and branch behavior. This is
a correction to reconstructed accesses, not an intentional gameplay policy.

## Validation and remaining scope

Changed files are `LPTargetComp.cpp`, `TargetObjectTemplate.h`, the
`Code/Tests/lp_target_native_runtime.cpp`/`.py` fixture, and this note.

The focused GCC 14.2 `-O2` fixture strictly links the complete dedicated
engine object set. Only an isolated copy of the main object has its entry
point renamed. Real target/component/template bodies execute; typed test
observers supply template lookup, network creation and message collection.
Cold abstract game-policy methods abort if reached. It does not start a
server or exercise network transport.

All 19 checks pass with the corrected component. Substituting only its saved
pre-fix object produces 8 failures: incorrect local/network creation,
incorrect local initialization, missing owner assignment and wrong laser
lifetime. Tests also retain TV/wire lifetime bypass, absent-target behavior,
the original network-creation arguments and unchanged object transforms.

Run after a strict Linux Ninja build with exported compile commands:

```sh
python3 Code/Tests/lp_target_native_runtime.py \
    --engine-build "$HOME/bf2-linux-build" \
    --output "$HOME/bf2-lp-target-check"
```

`--component-object` accepts a preserved pre-fix `LPTargetComp.cpp.o` for
the expected-failure control. Keep its output separate from the fixed run.

Completed build checks:

- Full MSVC x86 syntax gate: `ALL OK`.
- Isolated all-TU MSVC x86 `bf2_recovered` archive: passed. The initial
  parallel build hit an MSVC PDB write failure despite `/FS`; a serial retry
  completed. This is compile/archive coverage, not an x86 executable link.
- Isolated MSVC x64 `bf2_w64ded` strict link: passed.
- MSVC x86 `DEDICATED=0` real-header syntax check of `LPTargetComp.cpp`: passed.
- Linux x64 `bf2` strict link and `dice_py_check_run`: passed. Python checks
  covered 40 builtins, 64-bit pointer round-trip, zlib and loopback UDP.

The production run used copied stock content, normal
`BF2::init -> BF2Engine::startGame -> GameServer::startServer/loadPath`,
`strike_at_karkand gpm_cq 32`, and loopback game/query ports 17695/30695.
AI, internet, PunkBuster, VOIP and auto-recording were disabled. Loading
completed at 3.34 seconds; 15.17 seconds of observation followed. The server
quit normally and exited successfully at 19.15 seconds. Both test ports
rebound immediately over UDP and TCP. Two stock type-3 target initializations
returned non-null owned targets. No clients joined.

Local baseline files, logs and reproduction commands are under
`out/latency-vehicle-20261003`, with `fixture` and `runtime` subdirectories.
The Linux binary SHA-256 is
`2b213ddf6aa54075277a632fe502281ac33b6e780b394a7a9e494e2e07fafb80`.

No live retail-client session has yet established whether this correction
changes the reported aiming, helicopter or TV-missile symptoms. The jet
missile report remains unconfirmed; no seeker behavior has been changed.
To distinguish input backlog from a later firing/collision discrepancy, the
next reproduction needs the selected remote player's newest received input
index, queued/current index and queue depth alongside the launched gun
direction and collision target's compensation amount. A controlled retail
server comparison needs the same jet, target motion, range, ping and load.

Existing `TargetObject::addToNametagManager` and `removeFromNametagManager`
frontier declarations remain unchanged, as does the unresolved target word
at original ELF offset `+150`. The local-client auto-ping adjustment frontier
in `ClientConnection` is also unchanged; it does not execute for the reported
retail-client/remote-player dedicated-server configuration.
