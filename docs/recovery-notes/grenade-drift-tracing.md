# Opt-in grenade drift tracing

This temporary diagnostic supports [issue #20](https://github.com/B2BF/OpenBF2/issues/20), which originally reported sideways grenade drift with a retail client connected to the Linux rebuilt server. The reported video shows a smooth curve in the thrower's view on Gulf of Oman's airfield. Joined tests against the Windows rebuilt dedicated server captured four throws before a recovered deviation-scale correction and four afterward. The initial velocity deviation became smaller, but the user still reported lighter drift on two corrected-run grenades. The issue remains unresolved. Both server traces show straight horizontal motion after the initial deviation; the retail client's displayed correction remains an inference because no second-client view or client trajectory trace was captured.

The trace is **[CHOSEN, not recovered]**, requested by the user for the next normal test run. It observes existing values at launch, physics integration, collision, detonation and projectile network state processing. It makes no change to projectile physics or protocol. It is disabled unless `BF2_TRACE_GRENADES` is exactly `1` when the process first checks the setting.

## Enable it for a normal test

Use an executable rebuilt with this change. Set the environment in the same terminal that starts the executable; starting an already-running launcher does not update that launcher's environment.

Windows PowerShell:

```powershell
$env:BF2_TRACE_GRENADES = '1'
$env:BF2_GRENADE_TRACE_FILE = "$PWD\bf2-grenades.log"
# Start the rebuilt executable here with your usual test arguments.
```

The second setting is optional. Without it, the dedicated server writes `bf2-grenades-server.log` and the rebuilt client writes `bf2-grenades-client.log` in its working directory. Use separate explicit filenames when tracing both processes. Parent directories must already exist. An open failure is reported to stderr.

Linux shell:

```bash
export BF2_TRACE_GRENADES=1
export BF2_GRENADE_TRACE_FILE="$PWD/bf2-grenades.log"
# Start ./bf2 here with your usual test arguments.
```

The retail client cannot emit this instrumentation. A retail-client/rebuilt-server test therefore supplies a server trace and the client's video. For the next run, throw a few M67 grenades in the direction showing the curve, then return the log and the matching video with the build identity. An observer-client video, if available, provides an independent view. Keep the normal server configuration; agent-run tests must use an explicitly configured unused port other than the owner's reserved 16567.

To disable future processes, remove the environment variables before starting them:

```powershell
Remove-Item Env:BF2_TRACE_GRENADES, Env:BF2_GRENADE_TRACE_FILE -ErrorAction SilentlyContinue
```

```bash
unset BF2_TRACE_GRENADES BF2_GRENADE_TRACE_FILE
```

The enable setting is cached for the life of a process, so changing its parent terminal's environment does not change a running process.

## Read the log

The diagnostic selects only the template named `ushgr_m67_Projectile`, case-insensitively. It admits at most 16 selections and writes at most 8192 rows per process. Rows append to an existing file and flush immediately; choose a fresh filename to separate runs. Logging and flushing can affect timing, so use short diagnostic runs rather than interpreting them as performance measurements.

Each row carries `tick`, object ID, network ID and a process-local selection serial. A zero network ID means no assigned cached network identity was observed. Serial numbers restart with each process and are not global identities.

- `launch` records cached origin, projectile transform, weapon speed, throw strength, inherited velocity and the resulting launch velocity. This occurs before optional authored starting rotation and random spin are applied.
- `physics` records the existing integration step: current position, planned next position, post-acceleration velocity, acceleration and impulse inputs, integration velocity, drag, mass and gravity modifier. It precedes the existing position setter and does not claim to be the final transform after every subsequent system update.
- `collision`, `detonate` and `destroy` identify the corresponding instrumented engine boundaries and their existing values. Cache returns can bypass `destroy`; a reused object's new ID still separates its next selection. These records alone do not prove damage, score or a remote visual effect.
- `net_send` and `net_receive` record the selected state-ring pose, state mask, requested position/rotation channels and stream compression reference. A pose field is not sent by a delta whose corresponding bit is absent. Sender completion is not proof of packet delivery, and a receiver record is not proof of the final displayed transform. The projectile protocol has no transmitted velocity channel here.

The trace does not instrument the retail client's interpolation proxy, camera rendering, or every force setter. Retail client instructions inspected during this investigation show blending from the local proxy toward received projectile positions. That provides a mechanism for a smooth visible curve when the server follows a different trajectory; the four-throw server trace below does not directly observe that blending.

## Four-throw joined observation

The user completed four throws in the Windows dedicated test and reported two drifting left, one right, and an uncertain fourth. That description does not establish which serial corresponds to each screen direction. The captured server log is `out/grenade-drift-20261004/windows-live-20261004/logs/grenades.log`. Analysis used the immutable 525-line snapshot `analysis-native-scope/grenades.snapshot.log` under the same diagnostic directory, SHA-256 `6052f4f5983ffff5892bbf5c0857b55d834432fa18b76479d482706c790d0d81`. `analysis-native-scope/analyze.py` reproduces `analysis.json`.

Every throw starts at the logged launch position but has a changed velocity by the first physics step, before any logged collision. Initial acceleration and impulse are zero in all four cases.

| Serial | Launch tick | Launch X/Z velocity | First physics X/Z velocity | Horizontal angle change | First collision tick |
| --- | ---: | --- | --- | ---: | ---: |
| 1 | 4073 | -22.984978 / 6.338440 | -23.804731 / 3.204901 | +7.74917 degrees | 4110 |
| 2 | 4199 | -21.938141 / 9.184554 | -21.199678 / 10.540930 | -3.72058 degrees | 4238 |
| 3 | 4317 | -23.530228 / 3.167596 | -24.622019 / 3.010778 | +0.69545 degrees | 4344 |
| 4 | 4432 | -23.005060 / 5.339463 | -23.346245 / 2.988205 | +5.77304 degrees | 4474 |

The signed angle is the change in `atan2(vz, vx)`, normalized to the interval from -180 to +180 degrees; it is not a screen-left/right label. The launch speed is approximately 25 for each throw; first-step speeds are 25.20931, 25.05237, 25.23037 and 25.11920. Each velocity change is perpendicular to its original launch vector within the printed float precision: the launch-vector dot products with the changes are between `3.45e-6` and `2.82e-5`.

Before the first logged collision, the four traces contain 38, 40, 28 and 43 physics steps respectively. This boundary includes the physics row preceding a collision in the same tick. Horizontal velocity is constant throughout each interval, lateral acceleration and impulse remain zero, ticks have no gaps, and every next step starts exactly at the previous logged target position. All 24, 25, 17 and 27 requested preimpact network-position records respectively match that same tick's latest physics target exactly at logged precision. Drag is zero.

These observations rule out a continuously applied sideways force, an intervening logged collision, or a state-ring position mismatch as the explanation for these four preimpact server trajectories. They locate the initial discrepancy between the thrown launch calculation and first integration. They do not establish packet delivery or the position rendered by the retail client.

## Recovered deviation-scale mismatch

Fresh original Linux ELF instructions identify a concrete mismatch in the shared `DeviationComp::postFire` body at `0x825C430`. The two virtual calls at `0x825C6F0` and `0x825C779` use primary-vtable slot `+0x32C`. The `GenericFireArm` vtable at `0x88B4600`, with address point `0x88B4608`, has the corresponding entry at `0x88B4934`, resolving to `GenericFireArm::getVelocity() const` at `0x8253C20`. The subsequent divisions at `0x825C701` and `0x825C787` use the float constant 100 at `0x88844B8`.

The Windows dedicated PE confirms the same operation in `postFire` at `0x5C2C50`. Calls at `0x5C2DEA` and `0x5C2E41` use the `IWeaponObject` face's slot `+0x18C`; vtable entry `0x7D104C` resolves to the velocity getter at `0x5824C0`. The following instructions at `0x5C2DF0` and `0x5C2E47` multiply by `0.01f` (`0x3C23D70A` at `0x784978`). Both images scale each transform-axis component before multiplying by its deviation sample. `getDevMod` is a separate method, not an alternative name for this velocity query.

The reconstructed body instead called `getDevMod()` for both axes and omitted the division. That getter currently returns 1. The recovered operation requires two separate velocity queries, each divided by 100 before applying that axis's existing deviation. For the M67's authored velocity 25, the required scale is 0.25, so the previous code applied four times the intended additive deviation. This is a recovery correction, not an intentional change to retail spread policy. It preserves the existing random inputs, axis order and repeated virtual queries.

The corrected operation is shared by weapons using `DeviationComp::postFire`; it is not a grenade-specific clamp. No protocol, serialized layout or public interface change is required. The first four-throw measurements are consistent with this excessive perpendicular launch deviation. The corrected joined test below shows reduced deviation and residual reported visual drift. The explanation that retail prediction turns a trajectory difference into a smooth visible curve remains an inference without a client trajectory trace. No claim is made about the other helicopter, TV missile or hit-registration reports.

The recovery patch restores both velocity calls, percentage scaling and axis arithmetic order. Windows x86 client/server syntax checks, the x86 dedicated diagnostic link, the x64 strict link and the Linux strict link passed. The x64 executable was linked into `bin_w64ded_deviation` so the running pre-fix test executable could remain untouched. The first four-throw capture precedes the correction; the later capture below uses the corrected executable.

`Code/Tests/deviation_scale_runtime.cpp` strict-links the actual production deviation implementation and uses real projectile transforms, weapon velocity lookup and point-physics storage. Controlled game input and cached offsets isolate the spread scaling independently of random samples. All 52 checks pass with the corrected object; linking the preserved old object instead gives 17 failures. Cases cover velocities 25, 100 and 1000, identity and elevated transforms, two distinct velocity-query results, the existing threshold, dummy-projectile cache reuse and unchanged fire-deviation accumulation. The test also requires no `getDevMod` calls. MSVC x86 syntax validation of the fixture passes. The companion Python runner accepts `--engine-build`, `--output`, and an optional `--component-object` for this negative control. This is a focused runtime regression, not a joined-client result.

The corrected Windows x64 executable was subsequently started through normal `BF2::init` startup on loopback game port 16588, query port 29988, using an isolated copy of the Gulf of Oman 32-player Conquest content. Its query reply confirmed the map, `openplaying` state and game port before the user joined and performed the four throws described below. Tracing is enabled into `out/grenade-drift-20261004/windows-fixed-20261004/logs/grenades.log`. The original test session on 16587 was left unchanged for comparison.

## Corrected-server joined observation

The user joined the corrected server, PID 37876, and reported that two grenades still deviated, but more lightly. The new capture contains four complete throws. Analysis used the separate immutable 524-line snapshot `out/grenade-drift-20261004/analysis-fixed-native-scope/grenades.snapshot.log`, SHA-256 `11bb272d5d4415e3160221f3d078829cd15c8c6ca2f356a6604f5689f1457339`. Its `analyze.py`, `analysis.json` and `comparison.json` preserve this result without replacing the earlier run.

| Serial | Launch tick | Horizontal angle change | Full 3D angle change | First collision tick | Preimpact physics steps |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 4247 | +0.48857 degrees | 2.08905 degrees | 4275 | 29 |
| 2 | 4309 | -1.52509 degrees | 2.50981 degrees | 4339 | 31 |
| 3 | 4379 | -2.04825 degrees | 2.07180 degrees | 4411 | 33 |
| 4 | 4447 | -0.36297 degrees | 0.91572 degrees | 4495 | 49 |

Angles compare the logged launch velocity with the first physics velocity, using the same convention as the earlier table. The mean absolute horizontal change fell from 4.48456 to 1.10622 degrees, and the maximum from 7.74917 to 2.04825 degrees. These are independent four-throw samples with different aim and random draws, not a controlled replay or proof of an exact fourfold ratio. The source and focused regression establish the scale correction separately.

As before, each first step starts at the logged launch position with zero acceleration and impulse. The velocity changes are perpendicular to the launch vectors within logged precision. Horizontal velocity then remains constant until first contact, with zero lateral acceleration/impulse, no missing ticks and no position discontinuities. All 18, 19, 20 and 30 requested preimpact network positions respectively equal the latest same-tick physics target exactly at logged precision. This corrected run therefore does not show a gradually curving server trajectory or a mismatched sender state-ring position. It does not establish what the retail client predicted or rendered, and the user's lighter residual drift is not considered resolved.

## Live normal-table comparison

A read-only process-memory comparison captured the initialized normal-distribution tables from the running retail client and corrected Windows x64 server. The report is `out/grenade-drift-20261004/live-spread-tables.json`; each read covers only the table's 4096 bytes, containing 1024 floats.

| Process | Image MD5 | Table address in that process |
| --- | --- | --- |
| Retail client PID 62652, `D:\B2BF Launcher\Battlefield2\BF2.exe` | `26a0855a8881ba9e7414e5c7f7c74ab8` | `0x9F4DB0` |
| Corrected server PID 37876, `bin_w64ded_deviation\bf2_w64ded.exe` | `0f62b6acba21bc7aad931de3a749b3e4` | `0x7FF7908D5540` |

The client image identity was hashed from its running image's file path. The native server address was resolved from its link map and loaded module address; these addresses are observation provenance, not portable layout constants. Both tables were initialized. All 1024 float values match exactly, with maximum absolute difference zero and identical table SHA-256 `ca3fa5e70c9171bedfd00e5d4e6f0d08460bf55cbe3f86e810a5901a3d208d6d`.

Fresh original client instruction inspection identifies the constructor at `0x6239E0`, which seeds the generator with 135150 and fills 1024 normal samples, the getter at `0x5E7390`, and `DeviationComp::postFire` at `0x623770` using the table at `0x9F4DB0`. The inspected reference client's whole-file MD5 is `4c843dcee79388341f72c3a7adb1ace6`, different from the running client's file hash above. A separate read-only comparison verified exact instruction-byte equality for the relevant live ranges: constructor `0x6239E0` (119 bytes), normal getter `0x5E7390` (183 bytes), `postFire` `0x623770` (622 bytes), and `getCurrentInputIndex` `0x615910` (7 bytes). Evidence is retained in `client-original-instructions.json` and `client-live-instruction-comparison.json` under the diagnostic directory. This establishes those specific code ranges, not whole-image equality.

The live table equality rules out different initialized table contents for these two processes. It does not prove that client and server choose the same table indices, consume the same inputs, or launch a particular grenade with the same deviation. No original-client trajectory capture has been obtained, and the residual visual drift remains an open observation.

## Private retail-client observer

The ignored scripts `out/grenade-drift-20261004/observe-retail-grenades.py` and `retail_observer_memory.py` add a bounded external observer for the next four M67 throws, for at most 20 minutes. They use `ReadProcessMemory` only: no process writes, injected code, hooks or pauses. The active-weapon/fired-projectile field chain and projectile fields were checked against freshly inspected original client instructions, retained in `client-chain-instructions.json` and `retail-projectile-field-evidence.json`. All 26 combined instruction ranges matched the live client before the observer armed. Samples include projectile velocity, cached transform, the observed input index and timestamps. Reads are not atomic frame snapshots; a recorded dirty-cache flag warns that a cached transform may need refresh, and an observed input index is not guaranteed to be the exact launch index.

At the readiness timestamp of 09:54:30 Brussels time on October 4, 2026, observer PID 47088 targeted client PID 62652; the associated corrected server was PID 37876 on game port 16588. Output is `out/grenade-drift-20261004/retail-observer-20261004/grenades.jsonl`. At that recorded point, the log contained readiness only and the requested four additional throws were pending. This records the bounded session's setup, not a claim that the observer remains active or has captured trajectories.

## Implementation and validation boundary

`Game/Objects/Projectiles/GrenadeTrace.h` owns bounded per-image state and file output. Hooks live in `ThrownFireComp.cpp`, `PointPhysicsNode.cpp`, `GenericProjectile.cpp` and `GenericProjectileNetworkable.cpp`. The helper uses typed object/template/network interfaces and existing call-site locals. The temporary marker is `issue20-grenade-drift-diagnosed`: remove the helper and hooks after a joined-client trace identifies the cause and the resulting fix is validated.

The ignored fixture under `out/grenade-drift-20261004` strict-links the actual Linux engine objects. It explicitly selects two actual M67-named `GenericProjectile` objects, advances their actual `RotationalPointPhysicsNode`s for 12 steps, and injects a lateral acceleration only in the second trajectory at step 5. Its runner compares separate processes with tracing off and on, requires identical trajectory output and no trace file while disabled, and checks the 24 physics rows against the known velocity change. This validates the integration observation and cross-translation-unit trace state; it does not exercise launch, network, collision, client interpolation or joined gameplay.

Windows instrumentation validation passed the MSVC x86 dedicated diagnostic link and the x64 strict dedicated link, plus real-header syntax checks for all four instrumented translation units with both `DEDICATED=1` and `DEDICATED=0`. The x86 CMake target uses `/FORCE`, so its link is not a strict completeness gate even though the log contains no unresolved-symbol warnings. Separate four-throw Windows joined runs exercised the trace boundaries both before and after the deviation correction; the later run still has user-reported lighter visual drift.

The Linux dedicated strict link passed after rebuilding all four instrumented translation units. The actual-engine fixture then passed in both modes: disabled tracing created no file, enabled tracing produced exactly 24 physics rows for two object IDs/serials, and the complete trajectory stdout was byte-identical. The log distinguishes constant X velocity 11 from the fixture's tick-5 lateral acceleration 30, which changes X velocity to 12 with integration velocity 11.5; Z velocity stays -19. Logged position deltas match the integration values. This also proves the fixture's selection is visible to the separately compiled production physics translation unit.

The task-local runner is `native_trace_test.py --engine-build <existing Linux build> --output <new isolated directory>`. The completed run is under `/home/user/bf2-grenade-native-trace-20261004`; copied build/runtime evidence and `native-validation.json` are under `out/grenade-drift-20261004/native-validation`. No game or server was started by this fixture. Launch, network, collision/detonation and Windows execution were outside that fixture's scope; the later Windows joined traces supply separate observations of launch, physics, network sends, collision and detonation before and after correction. Client network receive/interpolation and the actual displayed trajectories remain unmeasured. The correction is validated at the recovered calculation boundary, but the residual gameplay report is not resolved.
