# Parachute aim preservation

The console bindings for `ObjectTemplate.maintainCameraOnEnter` and
`ObjectTemplate.maintainCameraOnExit` were missing. Both template fields default
to false, so stock scripts could not enable the existing camera-preservation
paths. Deploying a parachute while looking down consequently left the new camera
at its reset orientation instead of restoring the outgoing pitch.

Stock `Vehicles/Air/parachute/Parachute.tweak` in
the game's `mods/bf2/Objects_server.zip` sets both properties
to 1. The repair registers those existing Boolean properties in
`Code/BF2/Game/Objects/Game/ModuleWorldObjTemplGame.cpp`; it does not introduce a
new camera rule or change the parachute's body rotation.

## Original registration evidence

The original Battlefield 2 1.50 dedicated images were inspected directly in
read-only IDA sessions:

| Image | Architecture | Input MD5 |
| --- | --- | --- |
| `bf2` | ELF, x86 32-bit | `aa94014ed5d4308fb86655f5c890b59f` |
| `bf2_w32ded.exe` | PE, x86 32-bit | `3ca127b1be4275196af93347714707f9` |

| Property | Linux ELF | Windows PE |
| --- | --- | --- |
| `maintainCameraOnExit` | ConsoleClass1084 executor `0x863CFB0`, active predicate `0x863CD30` | registration `0x4B85B0`, executor `0x4B8700` |
| `maintainCameraOnEnter` | ConsoleClass1085 executor `0x863D430`, active predicate `0x863D1B0` | registration `0x4B8780`, executor `0x4B88D0` |

Both are Boolean set/get properties: zero arguments invokes the getter and
returns a Boolean; one argument invokes the setter without a return value. The
active predicate requires `getActiveTemplate(CID_PlayerControlObjectTemplate)`
to succeed (CID 50370). Neither has an explicit range. The ELF enter converter
at `0x863D2C0` parses a Boolean, and its range predicate at `0x863D1D0` returns
false. The PE converters at `0x4B86A0` and `0x4B8870` use Boolean extraction;
both active predicates resolve to `0x4C67C0`.

The ELF executors dispatch the existing primary virtual slots: exit setter/getter
`+0x230/+0x234`, enter setter/getter `+0x238/+0x23C`. The PE executors adjust to
the template's secondary interface at `+0x22C`, using exit slots `+0xFC/+0x100`
and enter slots `+0x104/+0x108`. Table `0x7D4FE8` resolves those to exit
`0x5794C0/0x5794D0` and enter `0x5794E0/0x5794F0`; these are plain byte
stores/reads of primary fields `+0x3FC/+0x3FD`. Production code uses the existing
typed methods, not those retail offsets.

Registration belongs to ObjectTemplate access 2, category 15. In the ELF, the
module's static initializer executes the category initializer through
`0x8584E8B` (`0x8593380`, constructor call `0x85933A2`) before the exit
and enter registration branches at `0x85852D7` and `0x85852ED`. Their name
assignments are at `0x858F709` and `0x858F5DD`. PE namespace initializer
`0x75F690` sets the same metadata before registration calls `0x75FCF5` and
`0x75FD15`. The source patch scopes category 15 to these two properties and then
restores the surrounding category 9.

## Existing camera path

`GameServer::enterVehicle` captures the outgoing camera orientation before the
old control object's `onEnter` and the new seat's `enter`. After entry it tests
the new template's maintain-camera property and, when enabled, restores the
middle orientation component as camera pitch and soldier look angle.

| Operation | Linux ELF | Windows PE |
| --- | --- | --- |
| Entry function | `0x80AF090` | `0x445080` |
| Capture old camera orientation | `0x80AF184` | `0x445125` |
| Notify old control object | `0x80AF24B` | `0x4451E2` |
| Read maintain-camera-on-enter | `0x80AF286` | `0x44520B` |
| Rebuild camera rotation | `0x80AF7FB` | `0x44529C` |
| Set current camera rotation | `0x80AF858` | `0x4452BD` |
| Set soldier look-up/down angle | `0x80AF889` | `0x4452DD` |

PE `Parachute::enter` at `0x599910` first delegates to
`PlayerControlObject::enter`, then preserves position while making the chute
body yaw-only. That matches the existing source. Restoring camera pitch occurs
in the caller; retaining pitch on the chute body would change retail behavior.

## Scope and validation

Only the two console bindings are restored. Defaults, virtual methods, camera
algorithms, signatures, inheritance, object layouts and packet formats remain
unchanged. Existing content can now select behavior already present in the
engine. No named frontier in the registration module is retired.

`Code/Tests/parachute_aim_runtime.cpp` exercises the real console registration,
Boolean parsing, ParachuteTemplate, GameServer entry, Soldier `onEnter` and Camera
transforms. Its narrow collaborators supply successful seat ownership, absent
animation instances and the required service/factory registry. The seat's actual
physics, weapon and AI assembly is outside the fixture. The camera remains a
child of the soldier, as in the stock setup.

The strict Linux GCC `-O2` fixture passed all 34 checks. Entry retained pitches
of +60 and -35 degrees. With the setting disabled, +60 reset to zero as expected.
The checks also cover independent entry/exit flags, Boolean queries, original
access/category metadata, excess arguments, an unrelated active template, and
the adjacent binding's unchanged category.

Replacing only the game-template module object with its preserved pre-fix version
produced 16 failures across 28 checks. Both enabled-entry cases reset to zero,
reproducing the reported symptom. The six registration/metadata checks requiring
the absent properties are skipped in that control; all three entry cases run.
Soldier entry still records the vehicle and clears skydiving in both versions.

Run from Linux against a configured, built dedicated tree:

```bash
python3 Code/Tests/parachute_aim_runtime.py \
  --engine-build ~/bf2-build --output ~/bf2-parachute-aim-test
```

The output directory must be outside the engine build. Use
`--module-object /path/to/preserved/ModuleWorldObjTemplGame.cpp.o` for a negative
control; the runner does not replace the engine's outputs.

Build checks passed: MSVC x86 C++14 permissive syntax for the changed module in
both client and dedicated configurations, MSVC x86 syntax for the regression,
Windows x86 dedicated compilation and diagnostic `/FORCE` link, and Windows
x64/Linux x64 dedicated strict links. Scoped whitespace checks also passed.

No server or client session was started. Map loading, physical parachute
deployment, networking, landing/exit and shutdown were not exercised. A
joined-client retest must still check deployment while looking up and down,
and exit behavior. Fixture and build success do not establish playable-client
validation.

The existing `[RETIRE-WHEN: client-entervehicle-seams]` marker in
`GameClient::enterVehicle` remains. Its removal requires live parachute
deployment and death on a ladder; an isolated server-entry fixture does not
satisfy that condition.
