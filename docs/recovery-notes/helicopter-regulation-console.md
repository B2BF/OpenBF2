# Helicopter stabilization configuration

The missing player-control template console properties prevented stock helicopter scripts from
enabling the existing yaw and vertical input regulators. This is a recovered configuration fix
relevant to [issue 16](https://github.com/B2BF/OpenBF2/issues/16) and
[issue 17](https://github.com/B2BF/OpenBF2/issues/17). It does not establish that all reported
descent behavior, or the stuttering in [issue 18](https://github.com/B2BF/OpenBF2/issues/18), is fixed.

`ModuleWorldObjTemplGame.cpp` now publishes the four `regulate*Input` properties, the four
`regulate*` gain pairs, and `maxVertRegAngle`/`noVertRegAngle`. These are dynamic properties taking
zero arguments to query or one to set, gated on an active `PlayerControlObjectTemplate`.
The mappings use the existing `io::PlayerInputMap` parser and advertise the inclusive 0–64 range
(`PINone` is 64). Gain pairs use `Vec2`; angle limits use `float`. The latter six properties have
no range or setter clamp. Existing console range semantics and parsing are preserved.

No physics formula, player-input regulator body, packet, interface, class layout, or default was
changed. This restores the shipped commands; it does not tune lift or add a new stabilization rule.

## Binary evidence

Fresh instructions were inspected in the original BF2 1.50 dedicated Linux ELF32 and Windows
PE32. The ELF static registration block at `0x859082A–0x85913F5` associates the names, types,
dynamic flag, zero/one argument bounds and vtables. Its executors call
`getActiveTemplate(CID_PlayerControlObjectTemplate)` and directly copy the fields below.

| Property | ELF console class | Executor | Original ELF32 field |
| --- | --- | --- | --- |
| regulateYaw | 1051 | `0x8635840` | `+0x230/+0x234` |
| regulatePitch | 1052 | `0x8635D40` | `+0x238/+0x23C` |
| regulateRoll | 1053 | `0x8636240` | `+0x240/+0x244` |
| regulateYawInput | 1055 | `0x8636950` | `+0x220` |
| regulatePitchInput | 1056 | `0x8637030` | `+0x224` |
| regulateRollInput | 1057 | `0x8637710` | `+0x228` |
| regulateVerticalPos | 1059 | `0x8637BE0` | `+0x248/+0x24C` |
| regulateVerticalPosInput | 1060 | `0x86382F0` | `+0x22C` |
| maxVertRegAngle | 1062 | `0x8638750` | `+0x250` |
| noVertRegAngle | 1063 | `0x8638BB0` | `+0x254` |

The vertical input active gate is `0x8637DF0`; its range methods at `0x8637E10/0x8637E20`
publish 0 and 64. `0x8638140` clamps only when requested by its Boolean argument.
The parser at `0x8638180` invokes the existing input-map stream operator. The vertical gain parser
at `0x8637A20` reads a float, one separator character, then another float.

The PE independently corroborates the critical vertical settings: constructor `0x4B67D0`
registers `regulateVerticalPosInput` as `io::PlayerInputMap`, dynamic, arity 0–1; executor
`0x4B6910` reads/writes template `+0x2F4`. `regulateVerticalPos` constructor `0x4B65B0`
uses `Vec2`, parser `0x4B66A0` reads the float/separator/float form, and executor `0x4B6750`
copies template `+0x310/+0x314`. These retail offsets document evidence only; the fix accesses
the existing typed members, whose native offsets may differ.

`PlayerControlObject::regulateInput` at ELF `0x819B3F0` reads these mappings and gains.
The vertical branch at `0x819BBDF–0x819BCE8` applies the two attitude thresholds, removes gravity
from positional acceleration, combines acceleration and speed gains, clamps the correction,
and fades it between the thresholds. Its existing source body was retained.
The engine input mapping and helicopter force calculation were also compared with
`EngineBase::handlePlayerInput` (`0x82A3090`), `mapInputToWantedAngle` (`0x82A2FA0`),
`Engine::handleUpdate` (`0x82A0A70`) and `updatePhysics` (`0x82A17E0`); this audit did not
justify changing those formulas.

## Regression and production checks

`Code/Tests/helicopter_regulation_runtime.cpp` links the complete actual dedicated engine objects
with a fixture main. A small class-manager adapter exposes real module factories. The test uses
real console parsing, template construction, `PlayerControlObject`, `PhysicsNode`, physics
manager and basic physics system. It does not override the recovered controller or its getters.
Only fixture-owned services and controlled physics state are supplied; no server is started.

The strict GCC 14.2 x86-64 `-O2` fixture passes **109 checks**. With the saved pre-fix console
module object, the same fixture exits unsuccessfully with **49 failed checks**: the commands
report `Unknown object or method!`, and stabilization stays disabled. Additional metadata checks
are skipped when the baseline registrations are absent, so total check counts differ.

In the controlled falling state (vertical speed −0.6, acceleration equal to gravity, upright,
stock gains), neutral throttle changes from **0 before the fix to 0.3 after it**. Explicit S and W
stay −1 and +1; yaw damping changes from 0 to −0.3. The fixture also covers constructor-disabled
regulation, pitch/roll, saturation, upward motion, unmapped controls, `PINone`, live angle changes,
full/faded/disabled correction at 30/45/60 degrees with stock 35/55 thresholds, excess arguments
and unrelated active templates. These are controller outputs, not measured descent rates.

Validation also passed the real-module MSVC x86 syntax checks for dedicated and client builds,
MSVC x64 strict dedicated engine link, and Linux strict `bf2` build plus Python import check.
No shared header changed.

The normal Linux `BF2::init -> BF2Engine::startGame -> GameServer` path loaded stock
`strike_at_karkand`, `gpm_cq`, 32-player content, with AI off and no client. It ran for 15.69 seconds
after load, quit normally and immediately rebound test ports 17697 and 30697 (TCP and UDP).
Read-only observations of loaded `ahe_ah1z` and `usthe_uh60` templates confirmed:

```text
yawInput=PIYaw (0), yaw=-0.2/0.2
verticalInput=PIThrottle (3), vertical=-0.5/2
maxVertRegAngle=35, noVertRegAngle=55
```

The authored 35/55 angles replace the constructor's 10/20 defaults. Startup evidence is in
`out/issues16-20-20261003/runtime/{validation.json,gdb.log}`. Fixture logs and baseline hashes
are in `out/helicopter-regulation-20261003/fixture/`. The first diagnostic fixture attempts only
repaired test setup (aggregate vector initialization and the required physics-manager singleton);
the final before/after runs use the same fixture.

No joined helicopter flight, client prediction, TV-missile steering, map reload or prolonged soak
was exercised. The missing settings are a proven defect and remove a concrete obstacle to stock
flight behavior; issues 16/17 need gameplay retesting and issue 18 remains a separate open question.
No named production frontier was removed or bypassed by this change.

## Reproduction

Use an existing complete Linux dedicated build with `compile_commands.json`. Preserve the old
module object **before** rebuilding to reproduce the negative control; do not overwrite engine
objects. The runner retains the configured compiler flags, strictly relinks an isolated executable,
and renames `main` only in an isolated copy of the existing BF2 main object.

```bash
build="$HOME/bf2-linux-bundle-nullcall/build"
out="$HOME/bf2-helicopter-regulation-20261003"
mkdir -p "$out"
cp "$build/CMakeFiles/bf2_engine_objects.dir/Code/BF2/Game/Objects/Game/ModuleWorldObjTemplGame.cpp.o" \
   "$out/ModuleWorldObjTemplGame.before.o" # before applying/rebuilding the fix
ninja -C "$build" -t commands bf2 | tail -n 1 > "$out/link-command.txt"

# After the normal fixed engine build has completed:
python3 Code/Tests/helicopter_regulation_runtime.py --engine-build "$build" \
  --output "$out/after" --link-command "$out/link-command.txt"
python3 Code/Tests/helicopter_regulation_runtime.py --engine-build "$build" \
  --output "$out/before" --link-command "$out/link-command.txt" \
  --module-object "$out/ModuleWorldObjTemplGame.before.o" # expected exit 1
```
