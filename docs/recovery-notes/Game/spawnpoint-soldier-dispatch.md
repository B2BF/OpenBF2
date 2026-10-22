# Spawn-point soldier occupancy dispatch

`SpawnPoint::getActive` checks nearby objects before making a spawn point
available. Its `CID_SoldierTemplate` branch incorrectly cast the object to
`PlayerControlObject`. `Soldier` and `PlayerControlObject` are separate
`Bundle` subclasses; implementing the same `IPlayerControlObject` interface
does not make their concrete primary virtual tables interchangeable.

On the native Linux build, the incorrect call used primary slot `+0x310`,
which belongs to `Soldier::enter`. That method received an invalid player
argument from a call that was supposed to have no arguments. This matches the
reported join crash in `Soldier::enter` while updating spawn-group state masks.
The correct soldier getter is at native primary slot `+0x300`.

The fix includes `Game/Soldier.h` and uses `static_cast<Soldier*>(hit)` after
the existing soldier class-ID check. It preserves direct virtual dispatch,
the null-occupant test and the alive-player test. A living occupied soldier
blocks the spawn point; a dead or unoccupied soldier does not. No interface
query, new guard, layout change, protocol change or asset-format change is
introduced.

The affected caller chain is
`GameServer::updateStateMasks -> NetworkManager::updateStateMasks ->
SpawnGroup::updateStateMask -> SpawnGroup::getActive -> SpawnPoint::getActive`.
It can run during a join without a player clicking a spawn button. Other
spawn-selection callers use the same corrected method.

## Original evidence

The BF2 1.50 originals were opened read-only in IDA:

- Dedicated ELF32 `ia-32`, MD5 `aa94014ed5d4308fb86655f5c890b59f`.
- Dedicated PE32 `bf2_w32ded.exe`, MD5 `3ca127b1be4275196af93347714707f9`.

In ELF `SpawnPoint::getActive` at `0x8115D20`, the soldier class-ID comparison
at `0x8115F30` selects the direct primary `+0x180` call at `0x8115F66`.
A non-null result is then tested through `IPlayer::getIsAlive` at `0x8115F77`.
There is no intervening `queryInterface` call. The decompiler's inferred
`PlayerControlObject` receiver type does not establish the receiver's class;
the class-ID gate and the soldier virtual table establish the call target.

The ELF soldier primary address point at `0x889FBC8` maps `+0x180` to
`Soldier::getOccupingPlayer` at `0x81B1950`; `+0x188` maps to `Soldier::enter`.
The getter returns the target of the weak block at `this+0x150`, or null when
there is no block. RTTI at `0x88A0140` identifies the four soldier bases:
`Bundle`, `IPlayerControlObject`, `IItemContainerObject` and `ISoldier`.

The PE uses a different dispatch arrangement. Its soldier player-control
interface table at `0x7CC590`, installed at `this+0x154`, maps interface slot
`+0x10` to the getter at `0x554C30`. That body reads the weak block at
`interface_this+0x14` and returns its target or null. These are different
primary and secondary tables; the ELF primary offset must not be applied to
the PE receiver.

PE `SpawnPoint::getActive` at `0x5812D0` confirms the caller: the soldier
class check at `0x581556` is followed by loading the secondary vptr at
`hit+0x154`, adjusting the receiver to that base, and calling interface
`+0x10` at `0x58156A`. A non-null occupant is tested through the player's
alive getter at `0x581575`. There is no interface query on this path.

These original x86 offsets document the recovered contract; the source uses
the actual C++ type so the compiler selects the native platform's slot.

## Validation

MSVC x86 real-header syntax checks pass for `SpawnPoint.cpp` with both
`DEDICATED=1` and `DEDICATED=0`, C++14, permissive mode, and forced inclusion
of `Core/BuildConfig.h`.

The incremental optimized `linux-x64` build of `bf2` and
`dice_py_check_run` passes with a strict link. The native function's
before/after disassembly differs only in the occupancy call displacement:
`+0x310` becomes `+0x300`. The native soldier virtual-table relocations
identify those targets as `enter` and `getOccupingPlayer`, respectively.
Evidence and `validation.json` are under
`/home/user/bf2-linux-bundle-nullcall/spawn-occupancy`.

The focused `Code/Tests/spawnpoint_soldier_dispatch_runtime` fixture passes
**33/33 checks** with GCC x86-64 and MSVC x86, both optimized and strictly
linked. It mechanically copies the current `getActive` and soldier getter
bodies into a test TU and compiles them against the actual engine class
declarations. Test-only constructors establish C++ object lifetimes without
engine registration; typed collaborators supply the grid and occupant, and
unused methods abort. This is a component test, not a production soldier
spawn or session test.

The cases cover a live occupant, dead occupant, missing weak block, expired
weak target, the same-root exclusion and the scatter bypass. The saved pre-fix
body aborts in the test's `Soldier::enter` observer instead of calling the
occupancy getter, reproducing the incorrect dispatch independently of the
server's invalid player pointer.

Repeat from the repository root with an unused output directory:

```bash
python3 Code/Tests/spawnpoint_soldier_dispatch_runtime.py ~/bf2-spawn-occupancy-check
```

```powershell
$env:BF2_BUILD_OUT = "$PWD\out\spawn-occupancy-check"
cmd /c Code\Tests\spawnpoint_soldier_dispatch_runtime.cmd
Remove-Item Env:BF2_BUILD_OUT
```

The runner accepts `--baseline <saved-SpawnPoint.cpp>` for the expected failing
comparison. Linux results are under
`/home/user/bf2-spawnpoint-soldier-fixture-20261003/{before,after-final}`;
Windows results are under
`out/spawnpoint-soldier-dispatch-20261003/windows-after`.

The rebuilt production server loaded `strike_at_karkand gpm_cq 32` through
`BF2::init -> BF2Engine::startGame -> GameServer::startServer -> loadPath`.
It reached 100% at about 3.4 seconds, ran another 25 seconds at about 35–36 FPS,
and exited normally via console `quit` after 29 seconds. The isolated run
used copied content, LAN/unranked settings, and disabled AI, PunkBuster and
VOIP. Game port **17691** and query port **30691** were free before launch
and rebound immediately after shutdown. Logs and cleanup verification are
under `out/spawnpoint-soldier-dispatch-20261003/runtime`.

No client joined that run, and the `getActive` breakpoint did not fire.
Coverage of the corrected occupancy branch comes from the regression fixture
and compiled dispatch inspection. Live joining, spawning, reconnecting and
map reload remain unverified for this change.

The pre-fix production TU is preserved locally at
`out/spawnpoint-soldier-dispatch-20261003/SpawnPoint.before.cpp`, SHA-256
`61cdaae5d5b8ab784629a3a611381da09e3811cf32e679c74315e81043be56f6`.
The corresponding optimized Linux object is preserved at
`/home/user/bf2-linux-bundle-nullcall/spawn-occupancy/SpawnPoint.before.o`.

Only `Code/BF2/Game/Objects/SpawnPoint.cpp` changes production behavior.
The accompanying changes add the focused `.cpp`, `.h`, `.py` and `.cmd`
test files and this evidence note; no source manifest changes are needed.
Existing spawn-selection frontier annotations and the documented shipped
interface/object mismatch in `getObjectToEnter` are outside this correction.
No unrelated frontier, interface declaration or original image was changed.
