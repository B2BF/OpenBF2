# Retail probe: a movie clip as an object's prototype (Boot Camp list)

This probe is for the gameswf S3 bridge (game-frontier group C2). Three code paths stop today when a
movie clip, rather than a plain object, sits on a prototype chain:
- `gameswf_action.cpp:2105` — `as_object::get_property`, retail SwiffPlayer.dll 0x10099EB0;
- `gameswf_action.cpp:2134` — `as_object::set_property`, retail 0x10099D80;
- `gameswf_impl.cpp:5308` — `sprite_instance::bind_class_prototype` during `on_event_load`, retail
  0x10081870 / 0x10081A12 / 0x10081A7E.

What the instructions show:
- Retail characters expose the script-object interface as a secondary base at +8.
- The accessor walk reads +0x28 (accessor hash) and +0x1C (inheritance link) relative to it, and the
  class load reads +0x20 (constructor).
- For a movie clip those offsets land on other members: the embedded arguments Array's member hash
  and its list links.

Statically, the outcome can't be told apart from garbage. This probe measures it.

## Content: one self-authored SWF, no game assets

SWF version 7 (AS2 semantics, as BF2's menus use), 640×480, two frames, a white background. Solid
60×60 squares (DefineShape) are placed on the main timeline as sprites named `ok1`..`ok9`, each with a
twin `bad1`..`bad9`:
- an `ok` square is green; a `bad` square is red;
- tests 1-5 sit in the top row, tests 6-9 in a second row, ok above bad;
- all eighteen start `_visible = false`.

A blue square named `done` (right of the second row) becomes visible when frame 2's script reaches its
end. If it never appears and the game is still running, the script stopped inside one of the tests.

Two DoActions, as ActionScript. Frame 1 sets up and attaches; frame 2 checks what the attached clips'
load produced (their class binding and constructor), then stops. `tools/swf/proto_probe.py`
generates exactly this, deterministically and from Python only: `probe_all.swf` and one SWF per test
(`swf`), the probe `Menu_client.zip` (`menuzip`), or a complete `protoprobe` mod copy (`mod`). The
source `mods/bf2` is only read.

Every write from inside a getter, setter or constructor names `_root` explicitly. Function declarations
capture no environment (SwiffPlayer.dll 0x10094AEA / 0x100952C9), so a bare name inside a function body
resolves in the *caller's* environment: for a constructor run by `on_event_load` that is the attached
clip, not `_root`.

```actionscript
// ---- frame 1 ----
// Test 1: ordinary member read through a movie-clip prototype.
var mc = _root.createEmptyMovieClip("protoClip", 100);
mc.plain = 42;
var o1 = new Object();
o1.__proto__ = mc;
var r1 = o1.plain;
if (r1 == 42) _root.ok1._visible = true; else _root.bad1._visible = true;

// Test 2: getter through a movie-clip prototype (as_object::get_property, 0x10099EB0).
var getCalls = 0;
mc.addProperty("g", function () { _root.getCalls++; return 7; }, null);
var o2 = new Object();
o2.__proto__ = mc;
var r2 = o2.g;
if (r2 == 7) _root.ok2._visible = true; else _root.bad2._visible = true;   // the getter's value proves it ran

// Test 3: setter through a movie-clip prototype (as_object::set_property, 0x10099D80).
var setValue = 0;
mc.addProperty("s", function () { return _root.setValue; }, function (v) { _root.setValue = v; });
var o3 = new Object();
o3.__proto__ = mc;
o3.s = 5;
if (setValue == 5) _root.ok3._visible = true; else _root.bad3._visible = true;   // the setter ran

// Test 4 (setup): class prototype that is a movie clip (bind_class_prototype, 0x10081870).
// The library sprite "probeSymbol" (an exported empty sprite) is registered with a class
// whose prototype is the clip. The attached clip's load reads the prototype's constructor field.
var ctorCalls = 0;
function ProbeClass() { _root.ctorCalls++; }
ProbeClass.prototype = mc;
Object.registerClass("probeSymbol", ProbeClass);
var inst = _root.attachMovie("probeSymbol", "probeInstance", 200);

// Test 5: addProperty on a movie clip itself, read directly (no prototype involved).
var c5 = _root.createEmptyMovieClip("directClip", 101);
var calls5 = 0;
c5.addProperty("d", function () { _root.calls5++; return 9; }, null);
var r5 = c5.d;
if (r5 == 9) _root.ok5._visible = true; else _root.bad5._visible = true;

// Test 6 (splits test 3's red): does the assigned value read back from the object?
var set6 = 0;
mc.addProperty("s6", function () { return _root.set6; }, function (v) { _root.set6 = v; });
var o6 = new Object();
o6.__proto__ = mc;
o6.s6 = 5;
if (o6.s6 == 5) _root.ok6._visible = true; else _root.bad6._visible = true;

// Tests 7 and 8 (split test 4's red): a class whose prototype is the clip, registered and attached.
var ctor7 = 0;
function ProbeClass7() { _root.ctor7++; }
ProbeClass7.prototype = mc;
Object.registerClass("probeSymbol", ProbeClass7);
var inst7 = _root.attachMovie("probeSymbol", "probeInstance7", 210);
if (inst7 != undefined) _root.ok7._visible = true; else _root.bad7._visible = true;   // 7: created

// Test 9 (setup; control for 4/7/8): the same attach with an ordinary class on a second exported sprite.
var ctor9 = 0;
function ProbeClass9() { _root.ctor9++; }
ProbeClass9.prototype = new Object();          // where test 4 assigns the clip
Object.registerClass("probeSymbol9", ProbeClass9);
var inst9 = _root.attachMovie("probeSymbol9", "probeInstance9", 220);

// ---- frame 2 ----
if (inst != undefined && ctorCalls == 1) _root.ok4._visible = true; else _root.bad4._visible = true;
if (inst7.plain == 42) _root.ok8._visible = true; else _root.bad8._visible = true;   // 8: clip member visible
if (inst9 != undefined && ctor9 == 1) _root.ok9._visible = true; else _root.bad9._visible = true;
_root.done._visible = true;
stop();
```

The library needs two exported, empty sprites: `probeSymbol` (tests 4, 7, 8) and `probeSymbol9` (test 9).

One SWF per test (`probe1.swf`..`probe9.swf`): run each alone, so a crash in one doesn't hide the others.

## Loading it in retail (a retail copy, never the owner's install)

1. Copy the retail install (BF2 1.50) in the Boot Camp VM.
2. Make a mod: copy `mods/bf2` to `mods/protoprobe`.
3. In `mods/protoprobe/Menu_client.zip`, replace `External/FlashMenu/mainMenu.swf` with the probe SWF,
   keeping the path and name. The engine loads `mainMenu` when the menu starts (SwiffHost
   `setFlaName mainMenu`).
4. Start `BF2.exe +modPath mods/protoprobe` (add `+fullscreen 0` for an easy capture).
   The menu's place shows the probe.
5. Capture the screen once `done` is visible (gshot), or note the crash or exit. A crash leaves the
   process gone; note the BF2 window state and any Windows Error Reporting dialog.

## Revision history: why tests 2-5 need a rerun (35e8807e and later)

The first retail run (de8d6d50, 2026-11-13, evidence `swf-proto-retail-20261113`) showed test 1 green
and tests 2-5 red, with `done` and no crash. In that revision the function bodies wrote bare names
(`getCalls++`, `setValue = v`, `ctorCalls++`, `calls5++`), so the reds are ambiguous:
- test 4's constructor runs in the attached clip's environment, so `ctorCalls++` landed on the clip and
  `_root.ctorCalls` stayed 0 even if the constructor ran; the check also ran in frame 1, before the
  attached clip's load;
- tests 2 and 5 also required the bare-name counter to be 1;
- test 3 checked `_root.setValue`, which a bare `setValue = v` in the setter may not have written.

What the first run does establish: retail survives every test (no fault), including addProperty on a
clip (test 5), and a plain member is found through a clip prototype (test 1). The native gameswf with
the same first-revision SWF and the control test 9 confirms the artifact: test 9 stayed red until the
writes named `_root` and the checks moved to frame 2.

## Reading the result

Each test shows either its green square, its red square, or nothing (stopped or crashed before it).
Tests 1-5 sit in the top row, tests 6-9 in a second row below them, and the blue `done` square on the
right.

| Test | Green (behaves like a normal prototype) | Red (runs, wrong value) | Nothing / crash |
|---|---|---|---|
| 1 plain member | movie members are visible through `__proto__` | members are not looked up in a movie prototype | the member lookup itself faults |
| 2 getter | (`r2 == 7`) retail finds a movie's accessor and calls it with the object as receiver | the accessor walk reads an unrelated (empty) table: no getter | the walk reads garbage and faults (the overlapping-field reading) |
| 3 setter | the setter runs (`setValue == 5`) | no setter found: the value lands as an own property of `o3` | fault in the walk |
| 4 class prototype | the constructor runs once with a movie as class prototype | the instance is created but the constructor field read finds nothing | fault in on_event_load |
| 6 value after a failed setter | `o6.s6` reads 5: the assignment became an own property (or a stored value) | the value is lost | fault |
| 7 instance created | `attachMovie` returns the instance | `inst7` is undefined: the attach fails with a clip as class prototype | fault |
| 8 clip member via class prototype | the instance sees `mc.plain` (the clip acts as its `__proto__`) | the clip's members are not visible on the instance | fault |
| 9 control: ordinary class | the attach path works here (instance + one constructor call) | the probe's attach path itself fails in this player: 4/7/8 say nothing about clip prototypes | fault |
| 5 addProperty on a clip | the clip's own getter works (`c5.d == 9`) | addProperty is accepted but the getter is not found | fault in addProperty itself (0x1009A0F0) |

What each result means for the rebuild:
- **All green.** Retail treats a movie clip as a full prototype. The rebuild forwards the accessor walk
  and the constructor read to the sprite's own script object, which is the typed equivalent of
  retail's secondary interface. The stops close by recovery.
- **Red, no crash** (the most likely for 2 and 3, given the overlapping fields). Retail silently finds
  nothing. The rebuild ends the walk at a movie prototype with "not found", and test 3's assignment
  becomes an own property. The stops close as recovered behaviour.
- **Nothing / crash.** Retail's behaviour is undefined (a fault). The rebuild keeps a labelled
  `[CHOSEN, not recovered]` safe answer, red's "not found", and records the retail crash as evidence.
- **Mixed.** Each path follows its own result: 2105 = test 2, 2134 = test 3, 5308 = test 4. Test 1
  checks that the setup works.

Record the screenshot, the BF2.exe and SwiffPlayer.dll versions (1.50), and for any crash the faulting
module and offset from Windows Error Reporting.

## Retail result (probe v2, 67788e01) and the rebuild

Retail 1.50 on Boot Camp, identical in the combined run and with each test alone, `done` every time,
no fault (evidence kept outside the repository, `swf-proto-retail-20261113/v2-67788e01`):
green 1, 4, 6, 7, 8, 9; red 2, 3, 5.

The rebuild (game-frontier batch 16, C2) follows it:
- the accessor walks (`as_object::get_property` / `set_property`) end "not found" at a movie clip;
  `get_member` still finds the clip's ordinary members, and a failed set stores an own member;
- a clip as class prototype binds the instance, runs the registered class once and links the clip as
  `__proto__`; the constructor field retail reads at the clip is unrelated movie state, so the rebuild
  publishes none [CHOSEN];
- `addProperty` on a clip receiver is accepted and kept nowhere [CHOSEN]; before C2 the native build
  dereferenced a null `cast_to_as_object()` there.

`bf2_swf_proto_probe_check` (`Code/Tests/swf_proto_probe_runtime.cpp`) generates the nine SWFs and
`probe_all.swf` and requires exactly retail's squares from the native gameswf.

### Before C2 (history)

Loaded in the native gameswf (`probe1.swf`..`probe9.swf`):
- Test 1 reached `as_object::get_property non-object prototype` (the plain member read walks
  the accessors first).
- Tests 4, 7 and 8 reached `sprite_instance::bind_class_prototype non-object prototype`.
- Test 9 (the control) was green.
- Tests 2, 3, 5 and 6 crashed in `addProperty` on the movie clip itself (test 5 without any prototype).

Test 5 isolates this. If retail shows test 5 green or red, retail survives addProperty on a clip, and
the native crash is a defect whose fix follows test 5's colour. If retail stops at test 5, retail faults
there too.

Shipped content does not reach it as far as the gates show. The retail menus call `addProperty`
(mainMenu.swf 18, loadGame.swf 16, endOfRound.swf 17, menu.swf 3 occurrences), and the menu, combat
and demo gates run them without a fault. That fits AS2 accessors declared on class prototypes, which
are plain objects. No shipped call on a movie clip receiver has been observed.
