# Vehicle kit-ability activation

`Player::updateAbility` implements the medic, support and engineer abilities
used while riding in a vehicle. It first requires the occupied control
object's `getAutoUseAbility()` to be true. Its first radius scan gathers nearby
occupied seats and appends their players' default soldier objects to the
ability targets. This allows the medic branch to heal friendly soldiers even
when those soldiers are attached to vehicles and excluded from ordinary
root-only object searches.

The corresponding `PlayerControlObjectTemplate::m_autoUseAbility` field
defaults to false. The reconstruction already had the field, constructor,
serializer, runtime getter and ability loop, but lacked the
`ObjectTemplate.autoUseAbility` console property. Vehicle scripts therefore
could not enable the behavior. A scan of the local retail `Objects_server.zip`
found 86 commands setting this property to 1: 85 under `Vehicles`/`Weapons`
and one under `freecontent`. The medic kit's vehicle radius,
strength and material commands were already registered.

The content scan used `mods/bf2/Objects_server.zip`, SHA-256
`8b515b84732b97756e8e429991e5ca427c6351df469f2642199b52258d56f3b8`.
For example, `Vehicles/Land/USJEP_HMMWV/USJEP_HMMWV.tweak` enables it on
`usjep_hmmwv`, `USJEP_HMMWV_CupolaBase`, `USJEP_HMMWV_Codriver` and
`USJEP_HMMWV_Rear_Passenger`. Vodnik and Nanjing seats also opt in.

The correction in `ModuleWorldObjTemplGame.cpp` restores the property using
the existing `GameTemplateConsoleAccess` friend. It accepts zero arguments
to return integer 0 or 1, or one integer argument to store `value != 0`.
It is available only when the active template implements
`CID_PlayerControlObjectTemplate`. It has no range restriction. This uses
the existing field and introduces no public method, layout, packet or asset
format changes.

## Original evidence

The original BF2 1.50 dedicated images were opened read-only in IDA, and
their binding registration, executor instructions and ability paths inspected:

- ELF32 `ia-32`, MD5 `aa94014ed5d4308fb86655f5c890b59f`.
- PE32 `bf2_w32ded.exe`, MD5 `3ca127b1be4275196af93347714707f9`.

| Contract | ELF32 | PE32 |
| --- | --- | --- |
| Property identity | `autoUseAbility` string `88FD6D6`, registration `858F805`, `ConsoleClass1083` table `8906688` | Constructor `4B8400`, instance `85A440`, initializer `75FCD0` |
| Integer argument, integer result, arity 0–1 | Registration `858F857–858F8AF`, parser `863C9D0` | Constructor `4B8400`, parser `4B84F0` |
| Active PCO-template gate | `863C8C0` | `4C67C0` |
| Setter normalizes nonzero; getter returns stored byte as int | Executor `863CB40`; store `863CB80` to template `+2EA`; read `863CBA4` | Executor `4B8550`; store `4B85A1` to template `+3E6` |
| Vehicle-ability processing | `Player::updateAbility` `80EA840` | `4794E0`; false auto-use gate `479565–47956D` |

The PE occupant scan calls `getOccupingPlayer` at `479728`, then
`getDefaultVehicle` at `479733`, and appends the soldier bodies to the second
scan at `4797C7`. Medic kit 5 selects same-team soldiers and applies healing
at `479B44`, dividing strength across the eligible targets. The ELF contains
the same two scans and occupant insertion. These behaviors were already
implemented; the missing configuration binding prevented their activation.

The console type is deliberately `int`, even though the stored field is
Boolean: inputs such as 2 and -1 enable it in the original. A Boolean parser
would not preserve this contract.

## Scope and validation

Held-medkit and dropped-medkit effects use different paths. The held-kit
radius routine (`825AB30` / `5A51E0`) uses root-only flags `01000400` and only
special-cases parachutes. Dropped-kit replenishment (`82916D0` / `5B8290`)
selects the first armor component in the candidate's parent chain. The
inspected implementations match those originals. This change restores
vehicle kit abilities; it does not expand held/dropped bags into a new
all-vehicle occupant healing policy.

MSVC x86 real-header syntax checks pass for the changed TU with
`DEDICATED=1` and `DEDICATED=0`, C++14, permissive mode, and forced inclusion
of `Core/BuildConfig.h`.

The Linux x86-64 dedicated engine rebuilt and linked strictly, and the
`dice_py_check_run` target passed. The focused
`Code/Tests/auto_use_ability_runtime.py` fixture links the actual dedicated
engine objects with an isolated copy of the entry-point object whose `main`
symbol is renamed. It runs its own test entry point without starting a
server or replacing production methods. All 28 checks passed using the real
console registration, integer parser, template factory and runtime getter.
Coverage includes the default value, 0/1/2/-1 normalization, getter results,
active-template restrictions and excess-argument rejection. Substituting
only the saved pre-fix module object fails the missing-registration check.

To run the focused fixture from the repository in Linux after a strict
dedicated Ninja build, use an output directory outside that build:

```sh
engine_build="$HOME/bf2-linux-build"
fixture_out="$HOME/bf2-auto-use-ability-test"
mkdir -p "$fixture_out"
ninja -C "$engine_build" -t commands bf2 | tail -n 1 > "$fixture_out/link-command.txt"
python3 Code/Tests/auto_use_ability_runtime.py \
    --engine-build "$engine_build" --output "$fixture_out/after" \
    --link-command "$fixture_out/link-command.txt"
```

The build must export `compile_commands.json`. To repeat the failure
control, preserve the module object before rebuilding the fix and pass its
path with `--module-object` into a separate fixture output directory.

A separate production-path run reached
`BF2::init -> BF2Engine::startGame -> GameServer::startServer/loadPath` with
copied stock content, `strike_at_karkand gpm_cq 32`, AI/internet/PunkBuster/
VOIP/auto-recording disabled, game port 17693 and query port 30693 on loopback.
Debugger observations recorded 86 executions of the property setter, each
storing true from input 1, including the HMMWV and Vodnik seats. The map
reached 100% loaded at 6.7 seconds, then ran for 15.2 seconds before a normal
console quit. The process exited successfully at 22.45 seconds; immediate
UDP/TCP rebinding on both test ports passed. This verifies stock content
ingestion and the unoccupied-server lifecycle, not a joined-player session.

Local regression logs and production-run evidence are retained under
`out/vehicle-ability-console-20261003/fixture` and
`out/vehicle-ability-console-20261003/runtime`. The Linux build is
`/home/user/bf2-linux-bundle-nullcall/build`; its console test outputs are
`/home/user/bf2-auto-use-ability-fixture-20261003/{before,after}`, and the
copied-content run is `/home/user/bf2-vehicle-ability-runtime-20261003`.

The pre-fix module source is saved locally at
`out/vehicle-ability-console-20261003/ModuleWorldObjTemplGame.before.cpp`,
SHA-256 `f234f7a79e72d836a074c058d7acfd02d3157c4124bafb01eaaaa2acfc9755a6`.

The changed files are `ModuleWorldObjTemplGame.cpp`, the console fixture's
`.cpp`/`.py` pair under `Code/Tests`, and this evidence note. No frontier is
added or retired by this correction.

The existing radius-query implementations and player scheduling are live;
old comments describing those queries as empty stubs are stale. This change
does not remove unrelated frontier markers or alter the dropped supply
crate's distinct hierarchy restrictions. A joined-player healing session is
still required to establish the end-to-end gameplay result.
