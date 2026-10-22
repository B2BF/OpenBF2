# Dropped ammo pickup with a full kit

`IWeaponAmmoComp::canBeRefilled()` returns `true` in the original engine. The
reconstructed default returned `false`, excluding ordinary weapons from
`Soldier::calcAmmoStatus()`. A full ordinary kit therefore reported ammunition
status 0 instead of 15, allowing a dropped ammo bag's preliminary replenishment
query to accept the soldier and consume the bag.

This method describes a component's refill capability, not whether its current
magazines need ammunition. `ReplenishingAmmoComp` deliberately overrides it with
`false`; that override must remain. Current ammunition fullness is calculated
separately, and actual refill methods determine how much ammunition is accepted.

## Original instructions

The original Battlefield 2 1.50 dedicated images were inspected directly in
read-only IDA sessions:

| Image | Architecture | Input MD5 |
| --- | --- | --- |
| `bf2` | ELF, x86 32-bit | `aa94014ed5d4308fb86655f5c890b59f` |
| `bf2_w32ded.exe` | PE, x86 32-bit | `3ca127b1be4275196af93347714707f9` |

| Contract | Linux ELF | Windows PE |
| --- | --- | --- |
| Default refill capability is true | `IWeaponAmmoComp::canBeRefilled`, `0x8259060`, returns `mov eax, 1` | `0x4921F0`: `mov al, 1; ret` |
| Replenishing component capability is false | `ReplenishingAmmoComp::canBeRefilled`, `0x825C140`, returns `xor eax, eax` | `0x72D320`: `xor al, al; ret` |
| Firearm delegates to its ammo component | `GenericFireArm::canBeRefilled`, `0x8254370`, interface slot `+0x9C` | `0x582700`, null check then tail call through slot `+0x98` |
| Soldier excludes non-refillable weapons from its gauge | `Soldier::calcAmmoStatus`, `0x81AF7C0`; rejection at `0x81AF81D–0x81AF822` | `0x54FD40`; call at `0x54FD8F`, false branch at `0x54FD94` |
| Query refuses a full gauge | `GameLogic::replenishAmmo`, `0x80D7E20`; getter at `0x80D7F04`, compare with 14 at `0x80D7F0F`, signed `jle` accepts | `0x467050`; getter at `0x46715F`, compare with 15 at `0x467165`, signed `jge` refuses |
| Bag requires a successful test-only query | `ReplenishDetonationComp::checkIfObjectCanTrigger`, `0x8291560`; query call at `0x82915A6` | `0x5B85A0`; query call at `0x5B85B9` |

The PE factory at `0x5A42D0` installs the DefaultAmmoComp interface table
`0x7D8548` at component offset `+0x10`. Its `+0x98` entry points to the true
body. The AmmoComp and interface base tables use that same body. The
ReplenishingAmmoComp constructor at `0x5A4E50` instead installs table
`0x7D88C0`, whose corresponding entry points to the false body. These table and
caller checks distinguish the two capabilities despite folded PE leaf functions.

`Soldier::calcAmmoStatus()` returns zero when no eligible magazine capacity is
counted; otherwise it returns the floored, clamped 0–15 magazine ratio. The server
updates the cached gauge during `Soldier::handleUpdate` (PE call `0x5533A0`,
store `0x5533A5`). The bag predicate first checks its ordinary trigger conditions,
then calls `doReplenish(candidate, true)`. A false result exits before retaining
the candidate and trigger tick. Its ammo arm forwards the query flag to game
logic (PE `0x5B8395`), where a full gauge is refused.

## Change and compatibility

The repair restores the default Boolean result in
`Code/BF2/Game/Objects/Weapons/AmmoComponents/AmmoComp.cpp` and corrects its
interface comment. It preserves the replenishing override, existing gauge
calculation, query threshold, actual refill behavior and pickup ordering. This
is recovered retail behavior, not a new pickup policy.

No signatures, inheritance, virtual order, object layout, serialized fields,
packet widths or content formats change. No unresolved boundary is implemented
or retired by this repair. The existing unresolved `AmmoComp::m_byte20` and
`m_lastFireTick` field semantics and `ReplenishingAmmoComp::m_unusedValues`
element type remain outside this change.

## Validation status

The focused fixture in `Code/Tests/ammo_pickup_runtime.cpp` links against the
actual dedicated engine objects. It runs the ammo component, firearm, Soldier
gauge, GameLogic query/refill and replenishment-trigger bodies. Only empty
animation rigs and an unobstructed collision ray are supplied as test
collaborators. It directly assigns the calculated gauge through `setAmmoStatus`,
without running movement or animation updates.

The strict Linux GCC `-O2` fixture passed all 29 checks. Replacing only the ammo
component object with its preserved pre-fix version completed all 29 checks with
13 expected failures, including a full kit incorrectly passing the bag query and
trigger. Cases cover a full kit, a missing spare magazine, refill followed by
refusal, an empty loaded magazine, a partially used loaded magazine, support
tools, infinite ammo and a weapon without an ammo component. Query-only calls
leave the ammunition unchanged. The partially used magazine case preserves the
retail magazine-count gauge rather than introducing a per-bullet pickup policy.

Run the fixture from Linux against a configured, built dedicated tree:

```bash
python3 Code/Tests/ammo_pickup_runtime.py \
  --engine-build ~/bf2-build --output ~/bf2-ammo-pickup-test
```

The output directory must be outside the engine build. The optional
`--component-object /path/to/preserved/AmmoComp.cpp.o` selects the negative
control without replacing any engine output.

Build validation passed:

- Full MSVC x86 `Code/Tests/syntaxcheck.cmd` gate (`ALL OK`).
- MSVC x86, C++14, permissive syntax checks of the client (`DEDICATED=0`)
  ammo component and the dedicated regression fixture (`DEDICATED=1`).
- Windows x86 `bf2_w32ded` build and diagnostic `/FORCE` link.
- Windows x64 `bf2_w64ded` and Linux x64 `bf2` strict links; Linux
  `dice_py_check` was current and successful.
- Scoped whitespace checks.

This validation did not start a server or exercise map loading, a retail-client
session, bag proximity scanning, detonation/removal, scoring or lifecycle
shutdown. An in-game retest should confirm that a full player leaves the bag
available for a second player who needs ammunition. Compilation and fixture
success alone are not a gameplay claim.
