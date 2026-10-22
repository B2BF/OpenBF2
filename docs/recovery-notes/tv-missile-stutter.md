# TV missile stutter investigation

The reported setup is an unmodified retail client connected to the OpenBF2 Linux
dedicated server. Whether the stutter requires steering is still unknown. This
investigation has not established a cause or a gameplay fix.

## Console and original-image checks

The stock AH1Z, Havoc and Z10 launch `agm114_hellfire_tv`. A strict-linked Linux
fixture exercised the real console dispatch, component factories and typed getters:
54/54 checks passed. It verified the stock seeker, follow, targeting, camera and
motor settings, and `BasicInfo` with linear prediction and base priority 1. The
fixture and logs are local investigation artifacts under
`out/tv-missile-audit-20261004/`; this was not a map load or joined-player test.

Fresh instruction inspection used the original 32-bit BF2 1.50 images:

- Dedicated PE MD5 `3ca127b1be4275196af93347714707f9`.
- Dedicated ELF MD5 `aa94014ed5d4308fb86655f5c890b59f`.

The ordinary TV path agrees with the current implementation at these boundaries:

| Boundary | PE | ELF |
| --- | --- | --- |
| TargetDirectionEvent server dispatch | `0x53E920` | `0x807F090` |
| SeekClosestTargetComp update | `0x5BA4F0` | `0x8294960` |
| SeekClosestTargetComp selection | `0x5BA1D0` | `0x82949E0` |
| Unique-target selection in GameLogic | `0x466C40` | inspected through seeker call |
| DefaultFollowComp ordinary TV steering | `0x5BAC90` | `0x8295E10` |

The ELF ghost update, transmission, commit/rollback and acknowledgement paths
(`0x8095920`, `0x8097FF0`, `0x809CAB0`, `0x8098A40`) and NetworkManager's minimum
size/mask adapter (`0x809BEF0`) also agree in the examined paths. Server timer
units match the ELF: elapsed wall-clock time is subtracted as a double before
narrowing to float seconds. These comparisons do not measure live scheduling,
packet loss, packet budgets, client interpolation or the reported camera stutter.

Two discrepancies outside this report's path remain: the rebuilt client's target
event send gate checks the projectile template name instead of networkable info;
the parented heat-seeking kill-distance arm samples the target rather than its
parent. Neither explains stock TV steering with a retail client, and neither is
changed here.

## Optional server trace

`TVMissileTrace.h` and hooks in `LPTargetComp.cpp`,
`GenericProjectileNetworkable.cpp` and `GhostManager.cpp` are temporary diagnostics,
`[CHOSEN, not recovered]`. They do not change guidance, console values, packet
formats, scheduling or object layouts. Remove them after a joined-client trace
identifies the cause and a fix is validated (`tv-missile-stutter-diagnosed`).

Enable before starting the rebuilt dedicated server, using its usual explicit
test port and configuration. Agent-run tests must use an unused port other than
16567; this investigation does not launch or replace the owner's server.

PowerShell:

```powershell
$env:BF2_TRACE_TV_MISSILES = '1'
$env:BF2_TV_MISSILE_TRACE_FILE = "$env:TEMP\bf2-tv-missiles.log"
# Start the rebuilt server with your usual arguments from this shell.
```

Linux:

```sh
export BF2_TRACE_TV_MISSILES=1
export BF2_TV_MISSILE_TRACE_FILE=/tmp/bf2-tv-missiles.log
# Start the rebuilt server with your usual arguments from this shell.
```

Use a retail client and fire several TV missiles: one straight without steering,
then another with repeated direction changes. Keep the client video and trace
from the same run. Exit the server normally to flush the final partial batch.
Unset `BF2_TRACE_TV_MISSILES` before starting subsequent sessions to disable it;
the enable flag is cached for each process.

Only the stock `agm114_hellfire_tv` name is selected, case-insensitively. Without
an explicit path the log is `bf2-tv-missiles.log` in the server's working directory.
It is created lazily on the first matching row and appended across runs; a `begin`
line separates each process's output. There is a process-wide limit of 8192 rows.
Output is buffered and flushed every 64 rows; forced termination can lose the
tail. Logging and clock reads have diagnostic overhead when enabled.

Each row contains steady-clock microseconds since the first matching row, world
tick, object ID and networkable ID. No player names or addresses are recorded.

| Event | Meaning |
| --- | --- |
| `steer_dispatch` | A steering ray reached the TV targeting component on the server, before target validation/placement. This is not a raw packet-receive timestamp. |
| `snapshot` | Current server replication record, including launch tick, pose and dirty mask. Multiple snapshots may precede a send; record time is not used as a clock. |
| `encode_candidate` | This projectile component encoded successfully into a stream. A surrounding descriptor or ghost write can still roll it back. |
| `ghost_selected` | The ghost update committed to that connection's packet/save bookkeeping. This does not prove UDP transmission, acknowledgement or client receipt. |
| `ghost_rejected` | The ghost write failed and will be rolled back. The failure is not necessarily packet-budget exhaustion. |

Mask bit 1 carries rotation; bit 2 carries position. Use `stream`, connection ID,
networkable ID and packet tick to correlate candidate and selected rows. Compare
snapshot motion and wall-clock spacing with selected-update spacing before
attributing the stutter to guidance or network scheduling. A packet capture or
client-side evidence is still needed to locate losses after packet selection.

## Validation scope

Validation completed:

- Actual-console fixture: 54/54 checks, including a repeat with the instrumented
  engine objects.
- MSVC x86 C++14 syntax/semantic checks of all three changed translation units
  with both `DEDICATED=1` and `DEDICATED=0`: passed.
- Windows x64 dedicated strict link: passed. Windows x86 Reconstruction compile
  and diagnostic `/FORCE` link: passed; this is not a strict-link claim.
- Linux x64 dedicated strict build and `dice_py_check_run`: passed.
- Strict-linked Linux trace-helper fixture: six process modes passed (20 grouped
  checks). Disabled values perform no access even with an unreadable object
  pointer and create no output. Enabled checks cover exact name filtering,
  case folding, shared state across two translation units, normal-exit flushing,
  monotonic timestamps and the exact 8192-row cap.
- Scoped `git diff --check`: passed.

Logs and temporary fixtures are under `out/tv-missile-audit-20261004/` and
`out/tv-trace-20261004/`. No server was launched and no retail client joined during
this investigation. No existing production frontier was removed or implemented;
the new trace retirement marker remains until the cause is measured and the
gameplay fix is validated.
