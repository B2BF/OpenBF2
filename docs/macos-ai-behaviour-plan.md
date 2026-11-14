# ai-behaviour-plan: recovering the behaviour and action-plan system (plan)

Branch `mac/ai-behaviour-plan` (from `mac-metal` 7b4399c). Planning and inventory only: no engine
code, no builds. The aim is a split that lets several agents recover the bot behaviour system in
parallel without editing the same files.

Machine-readable output (not committed; regenerate it with the tool below, numbers in this file come from it):

* `macos-ai-behaviour-functions.csv`: 2,984 functions. Columns: ELF address, size,
  instruction count, binding, translation unit, class, family, package, reach, min_tier, phase,
  tree status, demangled name.
* `macos-ai-behaviour-classes.csv`: 300 classes. Columns: family, package, reach, function
  and instruction counts, home TU, proposed file, base classes (from typeinfo), vtable address and
  word count, `operator new` sizes, ctors/dtors, and the classes each one uses (behaviour, tree, other).
* `tools/macos/ai_behaviour_inventory.py <libaidll.so> <workdir> <functions.csv> <classes.csv>`
  (env `TREE=Code/BF2` adds the tree-status column, `EXT=1` lists outside calls of the minimum
  set, `VT=ClassA,ClassB` dumps vtables slot by slot). It caches `objdump -t/-d/-R` in `<workdir>`
  (use `out/re`). It takes about 30 s.

## 1. Evidence and method

* Primary: `$BF2_LINUX_SERVER_DIR/bin/ia-32/libaidll.so` (Linux 1.5.3153, ia-32, with
  symbols). Functions come from `objdump -t` (`F .text`). Instruction counts come from
  `objdump -d`, with local `.Lnnn` labels kept inside their function. Classes come from the
  demangled names. Vtables are the `_ZTV` objects with their slots resolved through `R_386_32`
  and `R_386_RELATIVE` relocations. Base classes come from the `__si/__vmi_class_type_info`
  typeinfo. Object sizes come from `movl $N,(%esp); call _Znwj` followed by the constructor call.
* Translation units come from the FILE symbols. A file's range ends at its static initialiser.
  Its start is the symbol named in `_GLOBAL__I_<first symbol>`. Weak (linkonce) functions that
  directly follow a file's initialiser belong to that file, as GCC 3 places them. Strong
  functions that fall before a TU's first symbol come from a file that has no FILE symbol, i.e.
  no static initialiser (23 classes, see below).
* Source directories come from strings in both images (asserts and `__FILE__`):
  `AI/AIDLL/AI/AIBotActionPlan/` (BAPANotifyBot, BAPConLineOfFire, BAPWrapperWeapon),
  `AIBotBehaviourPattern/` (BBPFire2d, BBPFire2dDriver, BBPFireInfantery, BBPHeliFire3d),
  `AIBotBehaviour/` (BBTakeCoverInfantry, plus the anonymous namespaces of BBChange and
  VehicleUrgencyCalculator) and `AIPlanInterpreter/` (BAPInterpreter, BAPDebugger,
  ControlFunctions, InfanteryControl, CarControl, TankControl, EntryMoveToMedium*,
  EntryPlaneMoveToDirection). The PE strings read `C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\AI\AIDLL\AI\...`.
  Within each directory the link order is alphabetical, which confirms the TU names.
* PE cross-check (`AIDLL.dll`): the PE has no RTTI for these classes (only 3 `@dice@@` type
  names in the file). Its set of registration names is identical to the ELF's: 101 strings
  `BB*`/`BBP*`/`BAPST*` in each, with no difference. Per-function PE comparison is left to the
  packages, for x87 and ABI questions.
* Reachability is rapid type analysis. It follows direct calls. A class becomes instantiated
  when a reached function calls its constructor, and then all its vtable slots, ctors and dtors
  are reached. The seeds are `BotMain::planExecution/decisionMaking/pre-/postPlanExecution`, the
  `BAPInterpreter` load functions, and the classes the `.ai` content constructs (section 6).
  Limits: calls through function pointers are not followed. A class that is constructed only
  through a function pointer shows as unreached. The content mapping is taken from the shipped
  files, so it covers only what `mods/bf2` uses.
* "In tree" status scans `Code/BF2` for `Class::method(` definitions and `BF2_AI_FRONTIER` names.
  It is a heuristic, good for planning but not a gate.

## 2. Inventory

2,984 functions in the selection (the TUs from `BAPAAimAt.cpp` to `TankControl.cpp`, plus
`BotBehaviourManager.cpp` and `PrettyPrinter.cpp`, plus every class of the families below). Of
these, 526 are compiler- or STL-generated (472 static initialisers and `__tcf`, 54 `std::`
instantiations, about 10.2k instructions) and need no hand recovery. That leaves **2,458
functions, 297k instructions, in 300 classes** (290 with a vtable). The "about 1,700" estimate
from ai-nav counted the BAP/BBP families only. This count adds BB*, Entry*, the *Control
helpers and the factories.

| Family | Classes (examples) | Functions | Instructions |
| --- | --- | ---: | ---: |
| core | BotScript, BAPStatement, BAPAction, BAPCondition, BAPBoleanOp/And/Or/Not, BAPConTrue/False/Time/Iteration/SharedCondition, BAPVariable<T>, BotScriptVariable, BAPContext, PlanInterpreter, BAPInterpreter, PlanGenerator, BAPEntry | 119 | 4,479 |
| flow | BAPCombinerSerial/Parallel, BAPFlowCIf/While/Wait/End | 82 | 3,866 |
| action | 62 BAPA* (MoveTo*, AimAt*, Look*, Trigger*, SoldierPose, ...) | 543 | 24,321 |
| condition | 58 BAPCon*/BAPCCon* | 368 | 13,406 |
| wrapper | BAPInfoWrapper, BAPIW*, BAPIC*, BAPWrapper* | 105 | 6,159 |
| behaviour | BotBehaviour + 26 BB* | 216 | 43,158 |
| pattern | PlanGenerator/BBPPattern + 37 BBP* | 303 | 127,213 |
| urge | UrgeCurve, UCConstant/Linear/XInverse | 20 | 308 |
| entry | BAPEntry + 44 Entry* | 309 | 23,345 |
| control | Boat/Car/Heli/Plane/Tank/CommonControls (+ free functions in InfanteryControl, MouseControl, ControlFunctions) | 31 (+~60 free) | 16,939 |
| factory | BotBehaviourManager + 26 `BBCreateFunction*` (Main) | 38 | 14,910 |
| debug | BAPDebugger, BotScriptDebugger, PrettyPrinter | 33 | 3,418 |

The patterns hold most of the code. `BBPFireInfantery::createPlanInternal` (ELF 0x2650F0) alone
is 18,046 instructions (76 KB), and the whole BBPFireInfantery class is 31,112 instructions.

### Core class facts for the skeleton (from the ELF)

| Class | Base | Vtable (words) | new | Notes |
| --- | --- | --- | --- | --- |
| BotScript | - | 0x4B77D8 (3) | - | `+0 execute(Bot*, bool&, bool&)` pure, D1, D0. Inline. |
| BAPStatement | BotScript | 0x4BA240 (8) | - | execute, D1, D0, `getControls` +0x0C, `getId` +0x10 (pure), `hasDefault` +0x14 (pure), `debug(DebugLevel, string&)` +0x18, `streamSelf(ostream&) const` +0x1C |
| BAPAction | BAPStatement | 0x4B8000 (8) | - | inline |
| BAPCombiner | BAPStatement | 0x4B9080 (9) | - | + `addStatement(BAPStatement*)` +0x20 |
| BAPFlowControl | BAPStatement | (typeinfo only) | - | base of BAPFlowCIf/While/Wait/End |
| BAPCondition | - | 0x4BA1C8 (5) | - | `evaluate(Bot*)` +0, `getConditionType() const` +4, `streamSelf` +8, D1, D0 (the destructor is not first) |
| BAPBoleanOp | BAPCondition | 0x4B9E60 (6) | - | + `addCondition(BAPCondition*)` +0x14 |
| BAPVariable<T> | BotScriptVariable | e.g. `<bool>` 0x4BBD38 (2) | - | T in {bool, int, unsigned, float, Vec3, WeaponInfo*}; inline |
| PlanInterpreter | - | 0x4BCA60 (3) | - | D1, D0, `execute(BotScript*, Bot*, bool&, bool&)` +8 pure |
| BAPInterpreter | PlanInterpreter | 0x4BCA48 (3) | 0x10 | ctor(int); `addEntry(BAPEntry*)`, `addEntry(BotLodLevel, BAPEntry*)`, `removeEntry`, `activateEntry(BAPStatementType, BotLodLevel)` |
| BAPEntry | - | 0x4BCBE0 (4) | - | D1, D0, `execute(BotScript*, Bot*, bool&, bool&)`, `getTargetId()` (already in tree) |
| PlanGenerator | - | 0x4BC420 (6) | - | D1, D0, four pure slots |
| BBPPattern | PlanGenerator | 0x4BC400 (6) | - | `createPlan(Bot*)` +8 pure, `isInitiated` +0xC, `unInit` +0x10, `forceInitiated` +0x14 (concrete patterns add `getType()` +0x18) |
| UrgencyGenerator | - | 0x4B6E80 (10) | - | in tree (ai-nav) |
| BotBehaviour | UrgencyGenerator | 0x4BB580 (13) | - | ctor `(float*, UrgeCurve*, int)` 0x225250; `reset` +0x24, `calculateUrgency(Bot*, bool, float)` +0x28 pure, `generatePlan(Bot*)` +0x2C, `isAplicable(Information*, Bot*)` +0x30. BBFire adds two slots (15 words) |
| UrgeCurve | - | 0x4BB5E0 (3) | - | D1, D0, `calculate(float)`; UCConstant 8, UCLinear 0xC, UCXInverse 0x14 bytes |
| BAPAComponentSoldierPose | - | 0x4B7AE0 (16) | - | secondary base of BAPAMoveTo (BAPAMoveTo's vtable has 40 words, the second part at +0x58) |

Size bounds from `new`: BBIdle 0x30, BBMoveTo and BBAvoid 0x24, BBFire 0x40, BBFireInfantery 0x44,
BBPIdleInfantery 0xC, BBPFireInfantery 0x34, BAPCombinerSerial 0x24, BAPCombinerParallel 0x2C,
BAPFlowCIf 0x2C, BAPFlowCEnd 0x1C, BAPATrigger 0x30, BAPWrapperAiming 0xA8, BAPIWFire 0x34,
EntryInfanteryMoveTo 4, BotBehaviourManager 0x34. Every other class is in the classes CSV.

Type-id enums, read from the `getId`/`getConditionType`/`getType` bodies:

* `BAPStatementType` (`BAPStatement::getId`, 0..44): CombinerParallel 0, CombinerSerial 1,
  FlowCEnd 2, If 3, Wait 4, While 5, AimAt/AimAt3d 6, MoveTo/MoveTo3d 7, MoveToDirection/3dDirection 8,
  MoveToObject/3dObject 9, MoveAwayFrom3dObject 10, Trigger 11, TriggerContinously 12, Idle 13,
  ResetControls 14, InfoWrapper 15, AimingWrapper 16, FinishedWrapper 17, WeaponWrapper 18,
  LookAtWrapper 19, Sense 20, OverrideSensingAgent 21, PlaceC4 22, Look 23, Turn 24, Roll3d 25,
  SoldierPose 26, UpdateVehicle 27, ChangeWeapon 28, ICChangeVehicle 29, (30 unused by any class),
  ICTurnLock 31, Evaluate 32, NotifyBot 33, PhysicsEnable 34, SetSensingAgentFocus 35,
  MergeSoldierInputs 36, SendRadioMessage 37, Debug 38, ForgetTarget 39, UseFlareOnMissileLock 40,
  RunOverObject 41, HonkHorn 42, EvaluateCondition 43, Revive 44. The `.ai` files use the 44
  `BAPST*` names.
* `BAPConditionType` (0..58): LookDeviation 0, Precision 1, Precision3d 2, BombPrecision3d 3,
  And 4, EndOfPath 5, False 6, ..., TriggerableActive 58. The full list is in the CSV `getConditionType` rows.
  Three ids are shared by more than one class: PointInFront and ObjectInFront are both 23;
  ValidAiming, ValidPitchAiming and ValidYawAiming are all 32.
* `BotLodLevel`: the `.ai` files spell it `Full`/`Medium`; it is the third argument of
  `activateDefaultInterpreter`/`addInterpreterEntry`.

`BAPInterpreter::execute` (0x2A99D0, 209 instructions):

1. If `bot->vtbl[+0x118]()` returns true, it sets ok=true and failed=false and returns 1.
2. Otherwise it reads `id = script->getId()` (+0x10) and `lod = bot->vtbl[+0xC]()`.
3. If bit `id` of the 64-bit mask `this+8[lod]` (`activateEntry`) is set, it calls
   `entries = this+0xC[id][lod]`. A non-null entry runs `BAPEntry::execute`. Without an entry
   the statement's own `execute(bot, ok, failed)` (slot 0) runs.

The mask and the entry table are filled by `aiSettings.activateDefaultInterpreter` and
`addInterpreterEntry`.

### Files without a FILE symbol (propose class-named files)

These 23 classes sit in TUs without a static initialiser, so their file names are not
attested: BAPAEvaluate, BAPAIdle, BAPASense, BAPASoldierPose, BAPFlowCEnd, BAPFlowCWait,
BAPConIteration, BAPConSharedCondition, BAPConTime, BAPConObjectManDown, BAPConTotalAmmo,
BAPConWeaponHeat, BAPConWeaponMagAmmo, BAPConWeaponTotalAmmo, BAPConAnd, BAPBoleanOp, BAPConNot,
BAPConOr, BAPIWRoam, BAPContext, BAPWrapperWeapon, UCConstant, UrgeCurve, plus IPIControlInfo
(AIKnowledge). Propose `<Class>.cpp` for each, marked `[INFERRED]` file name. Twenty classes have
only weak (inline) functions and live in headers: BotScript, BAPAction, BAPCombiner, BAPBoleanOp,
BAPConTrue/False, BAPConOpLessThan<float>, BAPVariable<T>, BotScriptVariable, PlanGenerator,
PlanInterpreter, BAPEntry, BBPGotoWaypoint, BotScriptDebugger, UrgeCurve, and the
`BBCreateFunction*` (in `Main/BotBehaviourManager.cpp`'s anonymous namespace). Attested
multi-class TUs: `UCLinear.cpp` (UrgeCurve, UCConstant, UCLinear), `BBPGotoWaypoints2d/3d/Boat/Infantery.cpp`
(class names are `BBPGotoWaypoint2d/3d/Boat/Soldier`), `BBPPattern.cpp` (+PlanGenerator),
`BAPInterpreter.cpp` (+PlanInterpreter), `BAPDebugger.cpp` (+BotScriptDebugger).

The CSV `proposed_file` column gives the file for each class. New code goes to
`Code/BF2/AI/AIDLL/AI/{AIBotActionPlan, AIBotBehaviourPattern}/` (new directories) and to
`AIBotBehaviour/` and `AIPlanInterpreter/` (existing directories, owned by ai-nav today).

## 3. Runtime model and reachability order

Load time. `ai/aibehaviours.ai` runs through `AIConsole`/`AISettings` (Main, owned by ai-main):

* `setVehicleBehaviour` goes through `BotBehaviourManager::create` and then the
  `BBCreateFunction<X>::operator()`. Each create function does `new BB<X>(..., new BBP<Y>(id))`.
  The pattern is chosen by a string-compare chain.
* `addInterpreterEntry`/`activateDefaultInterpreter` go to `AISettings` and then to
  `BAPInterpreter::addEntry/activateEntry`. The interpreters come from
  `AISettings::preparePlanInterpreterPool` (0x3BFDE0).
* The `Entry*` objects are constructed in `AIConsole::AIConsole` (0x383470) and looked up by
  `getBAPEntryFromName` (0x392610). There are 44 Entry classes; the content names 41 of them.

Every behaviour and pattern named in the content is therefore constructed at map load, for
every vehicle type, before any bot spawns. Today these console commands are frontiers or not
registered (ai-main is waiting for `BAPInterpreter`, see `coord/ai-requests.md`).

Bot tick:

1. `BotMain::decisionMaking` (0x1893D0) computes the urgency of every behaviour of the bot's
   vehicle through `UrgencyMerger::updateActiveUrgency` (0x1B5A40, in tree). That loop calls
   `updateActiveUrgency` (+0x10) on every generator. In BotBehaviour this runs
   `calculateUrgency` (+0x28, per BB class); `isAplicable` (+0x30) filters information.
2. It then picks a behaviour, calls `generatePlan` (+0x2C) and `getChosenGenerator` (+0x20),
   and from there `BBP*::createPlan` (+0x08). That builds a BAP tree: Combiner and FlowC nodes,
   BAPA* actions and BAPCon* conditions.
3. The tree is stored with `setPlan` (BotMain +116, `m_activePlan`).
4. `BotMain::planExecution` (0x188B90) calls `PlanInterpreter::execute` (+8) on the active plan
   through `BAPInterpreter` (BotMain +144).
5. The interpreter dispatches to an `Entry*::execute` (the vehicle-specific control, which uses
   the `*Control` helpers and `IPIControlInfo`) or to the statement's own `execute`. Combiners
   recurse into their children; conditions evaluate against the bot, its senses and its
   knowledge.
6. `SensingAgent`'s patterns (AIBot, ai-bot) build small plans of their own from BAPALook,
   BAPALookAtObject, BAPALookInDir, BAPAMergeSoldierInputs, BAPAResetControls,
   BAPATriggerContinously and BAPCombinerParallel.

Every behaviour of a vehicle is scored on every decision tick. The urgency code of all
infantry behaviours is therefore on the tick path even when only Idle, MoveTo and Fire are ever
chosen. `generatePlan` runs only for the chosen behaviour.

Reach tiers (CSV `reach` + `min_tier` + `phase`, hand-recovery functions only):

| Tier | What | Functions | Instructions |
| --- | --- | ---: | ---: |
| min / stand-move | tick with BBIdle+BBPIdleInfantery, BBMoveTo+BBPGotoWaypointSoldier, the 13 infantry entries, BAPInterpreter, UC* | 467 | 30.6k |
| min / shoot | adds BBFireInfantery+BBPFireInfantery | 399 | 54.0k |
| infantry | the other 9 infantry behaviours (Avoid, MedicAssist, BridgeRepair, C4, DefuseMines, Change, Revive, Random, Triggerable) and their patterns | 299 | 56.4k |
| vehicle | all other content (Tank, Plane, Boat, Car, LandingCraft, Heli, Fixed, ...) | 530 | 99.9k |
| sensing | only SensingAgent reaches it (BAPALookInDir) | 10 | 0.3k |
| unreached | TakeCover family (no content uses it), BBPFire2dDriver, Roam, Suicide, CallForMedic, debugger, PrettyPrinter, unused statements/conditions | ~480 | ~43k |

"Phase load" functions (constructors and destructors of content behaviours, patterns and
entries, the factories, interpreter registration) total about 40k instructions across packages.
They must exist before the first tick: aibehaviours.ai builds all of them when the map loads.

### Needed for "bots stand, move and shoot" (minimum set)

* Load: `BBCreateFunctionIdle/MoveTo/FireInfantery` (+ all other create functions the content
  calls, M), AISettings/AIConsole interpreter and behaviour commands (M), BAPInterpreter
  add/activate (A), constructors and destructors of every content BB/BBP class (D, E, F, G
  milestone 1: about 26k instructions), UC* (A).
* Tick, stand+move: BotBehaviour, BBPPattern, BAPStatement/Condition, the combiners and flow
  nodes, BAPConAnd/Or/Not/Shared/Time (A); BBIdle, BBMoveTo, BBPIdle, BBPIdleInfantery,
  BBPIdle2d/3d `init`, BBPGotoWaypoint, BBPGotoWaypointSoldier (D); the 13 infantry entries plus
  InfanteryControl, MouseControl and ControlFunctions (B); BAPAMoveTo, MoveToFinding,
  MoveToObject, MoveToObjectFinding, ComponentSoldierPose, SoldierPose, ResetControls, Trigger,
  SetSensingAgentFocus, InfoWrapper, IWIdle, IWMoveTo, BAPConObjectMoved, BAPConPosition,
  BAPConWeaponIsZoomed (C).
* Tick, shoot: BBFire (`calculateUrgency` 2,089 and `chooseWeapon` 1,258 instructions),
  BBFireInfantery, BBPFire (D); BBPFireInfantery (E); AimAt*, Look*, MergeSoldierInputs,
  OverrideSensingAgent, ChangeWeapon, ForgetTarget, UseFlareOnMissileLock, EvaluateCondition,
  WrapperAiming/LookAt, IWFire, ICTurnLock, CConPrecision/LookDeviation and 17 weapon or object
  conditions (C).
* Urgency of the 9 other infantry behaviours (F milestone 1: about 10.9k instructions of
  `calculateUrgency`/`isApplicable`/helpers).
* Outside the system (other agents; `EXT=1` output, "absent" = no definition found in `Code/BF2`):
  * ai-bot: Aimer (7 functions: ctor/dtor, setTargetData, getFiringDirection,
    getStaticFiringDirection, getTurnPointForGrenade, getImpactPosition; `Aimer.cpp` is not in
    the tree). BotNavigation `initPathfinding` x2, `updatePath`, `updateLastPoint`, `setIsStuck`,
    `getSecondPoint`, `getLargestAngleDiff`, `getFirstPointBeyondDist`,
    `pointWithinObstacleRadius`. BotMain `getPlanInterpreter`, `getHarmlessThrsh`,
    `setInvAttention`, `setHadPrecisionTimeStamp`, `Input::setInput`. BotSenses `forgetObject`.
    SensingAgent `insertPlanTarget/DirectionInformation`. SquadMember `clearRequestAction`,
    `setIsIdle`, `isAtFormationSlot`, `getMaxSpeed`, `isAvoidingArtillery`. SquadManager
    `shotsFired`, `isInArtilleryDangerZone`. SquadFormation `shouldSquadMoveOut`.
    SquadCoordinator `getValidSaPos`. CommunicationAgent `enqueueRadioMessage`,
    `getReceivedMessages`, `confirmReceivedMessage`, `generalConfirmReceivedMessage`. Also the
    BotMain/Bot vtable slots the actions call indirectly (not enumerated here).
  * ai-nav: IPIControlInfo (12 accessors: get{Drive,Aim,Look}Controls,
    getLook{Horizontal,Vertical}Control, get{Proportional,Derivative,AngleSpeed}Constant,
    getCamera{BasePos,Transformation}, validateCameraDirection; file without a FILE symbol),
    IgnoreListPredicate ctor, CustomCurve (ctor, dtor, `calculate`: the file is in the tree but
    these three are missing; the image's path is `AIUtils/CustomCurve.cpp`), `TraceUtils`.
  * ai-strategic: AIStrategicArea `getControlpointPos`, `isInside`.
  * Engine/shared: `getSignedDiffAngleToDirXZ/YZ` (vector2dTools), `Debug::Debug`,
    `operator<<(ostream&, PlayerInputMap)`.

## 4. Packages

All counts exclude compiler-generated code. "Insns" is a size proxy; recovered C++ is roughly one
line per 3-5 instructions here.

### S0 skeleton (first, one agent; everything else waits for it)

**Done on `mac/ai-s0-skeleton`; what it actually contains is in section 8.**

Goal: every class exists, links and stops by name, so the packages fill bodies in disjoint files.
Write a generator, `tools/macos/ai_behaviour_skeleton.py`, on top of the inventory tool. Then:

1. Base headers, with layouts named from the ELF constructors: BotScript, BAPStatement,
   BAPAction, BAPCombiner, BAPFlowControl, BAPCondition, BAPBoleanOp, BAPVariable<T>,
   BotScriptVariable (move it out of `BAPContext.h`), PlanInterpreter, BAPInterpreter,
   PlanGenerator, BBPPattern, BotBehaviour, UrgeCurve (already in tree; keep it), and
   `AIBotActionPlan/BAPTypes.h` with `BAPStatementType`, `BAPConditionType`, `BotLodLevel` and
   the pattern `getType` ids. These must be complete, because every package derives from them.
2. One header per concrete class (all 290 vtable classes). Virtual declarations follow the ELF
   vtable order, so the generator emits them from the slot dump. Constructor signatures come
   from the mangled names. The `new` size and vtable address go in a header comment. Data
   members stay as `[UNRESOLVED]` raw storage (`unsigned char m_raw[N - base]`) until the owning
   package names them. The owner may rename and retype its own class's members; base layouts
   are frozen after S0 and change only through a request.
3. One `.cpp` per attested TU (`proposed_file`). Every non-inline method gets a
   `BF2_AI_FRONTIER("dice::hfe::ai::X::m")` body. Inline classes (49 functions, 1.3k insns)
   are recovered fully in S0.
4. Add every new `.cpp` to all three source lists in one commit: `CMakeLists.txt` (the list near
   line 223 and the `bf2_aidll` list near line 1275) and `Code/Build/aidll.cmd`. **After S0 no
   package edits build files.** A package that needs a new file asks the coordinator.
5. Split `AIPlanInterpreter/BAPEntries.cpp/.h` into the attested `Entry*.cpp` TUs (one per
   class). The bodies already recovered move unchanged.
6. A check, `tools/macos/ai_behaviour_vtables.py`, that compares each header's virtual order
   (by slot) with the ELF vtable dump. It runs on demand, not in the build.

Acceptance: `bf2_aidll` and `bf2` build and link (strict, `-j2`); `bf2_vtable_audit` stays
green; the vtable check passes for all 290 classes; the loopback server run stops at the same
place as before, or later at a named behaviour frontier. Estimated size: about 300 headers and
about 200 `.cpp` stubs, mostly generated; hand work goes into the about 15 base layouts.

### Wave 1 (parallel, after S0): A to F

| Pkg | Scope (classes) | Files | Fns | Insns (min/tick share) | Owner dir |
| --- | --- | --- | ---: | ---: | --- |
| **A core** | BotBehaviour, BBPPattern, BAPStatement, BAPCondition, BAPCombinerSerial/Parallel, BAPFlowCIf/While/Wait/End, BAPConAnd/Or/Not/SharedCondition/Time/Iteration, BAPInterpreter, BAPContext, UCConstant/Linear/XInverse, VehicleUrgencyCalculator | AIPlanInterpreter/{BAPInterpreter,BAPContext}.cpp, AIBotActionPlan/{BAPStatement,BAPCondition,BAPCombiner*,BAPFlowC*,BAPCon{And,Or,Not,SharedCondition,Time,Iteration}}.cpp, AIBotBehaviour/{BotBehaviour,UCLinear,UCXInverse,VehicleUrgencyCalculator}.cpp, AIBotBehaviourPattern/BBPPattern.cpp | 229 | 12.2k (8.6k) | new + ai-nav's dirs |
| **B entries** | the 13 infantry entries (InfanteryMoveTo/ToObject/ToDirection/ResetControls, MoveToMediumSoldier, MoveToObjectMediumSoldier, InfoWrapper, Trigger, TriggerContinously, MouseTurretAimAt/LookAt, Sense, SoldierPose) + InfanteryControl, MouseControl, ControlFunctions | AIPlanInterpreter/Entry*.cpp (13), {InfanteryControl,MouseControl,ControlFunctions}.cpp | 130 | 10.3k (7.8k) | AIPlanInterpreter (from ai-nav) |
| **C actions** | 52 BAPA/BAPCon/BAPCCon/BAPWrapper/BAPIW/BAPIC classes of the minimum set (list in CSV, package `C-actions`); the 7 SensingAgent ones first | AIBotActionPlan/*.cpp (52 files) | 550 | 26.3k (24.7k) | new |
| **D behaviours** | BBIdle, BBMoveTo, BBFire, BBFireInfantery; BBPIdle, BBPIdleInfantery, BBPIdle2d, BBPIdle3d, BBPGotoWaypoint, BBPGotoWaypointSoldier, BBPFire | AIBotBehaviour/{BBIdle,BBMoveTo,BBFire,BBFireInfantery}.cpp, AIBotBehaviourPattern/{BBPIdle,BBPIdleInfantery,BBPIdle2d,BBPIdle3d,BBPGotoWaypoints2d (base only),BBPGotoWaypointsInfantery,BBPFire}.cpp | 87 | 15.1k (11.7k) | new |
| **E fire pattern** | BBPFireInfantery (+Data, FiringState) | AIBotBehaviourPattern/BBPFireInfantery.cpp | 21 | 31.6k (31.3k) | new |
| **F infantry** | the 9 other infantry behaviours, 10 patterns and the 19 actions/conditions/wrappers only they use (CSV `F-infantry`). Milestone F.1: all BB urgency code and the ctors/dtors of every content BB/BBP, including G's classes (about 26k insns). F.2: the `createPlan` bodies and their actions (about 32k) | AIBotBehaviour/BB{Avoid,MedicAssist,BridgeRepair,C4,DefuseMines,Change,Revive,Random,Triggerable}.cpp, AIBotBehaviourPattern/BBP{...}.cpp, 19 AIBotActionPlan files; G's BB/BBP files for ctor/dtor bodies only (in wave 1, handed over to G at the wave boundary) | 338 | 55.1k | new |

`BBPGotoWaypoints2d.cpp` holds the inline base BBPGotoWaypoint (S0) and BBPGotoWaypoint2d (G).
D touches no function in that file. `BBPIdle2d/3d` move to D because
`BBPIdleInfantery::createPlan` calls their `init` directly.

Acceptance per package:

* A: unit behaviour per ELF (combiner serial/parallel completion rules, If/While/Wait/End, Not/And/Or);
  with M done, `aibehaviours.ai`'s 452 interpreter lines load without a stop; a bot tick passes
  `planExecution` with an empty plan.
* B: the 12 infantry `execute` stops of today's `BAPEntries.cpp` gone (it has 41 stops: 40 `execute` plus `EntryTankTurnTo::getCentreOfFacingSide`; `EntryInfanteryMoveToDirection::execute` is already recovered; EntryPlaneRoll and EntryTurnToMedium are not in the tree yet); the Infantry `IPIControlInfo` paths work
  (needs ai-nav's 12 accessors).
* C: the 7 SensingAgent classes first (tell ai-bot); all 52 without stops; `BAPAMoveTo*` against
  `BotNavigation` (needs ai-bot's 9 methods).
* D: run: coop bots spawn on dalian_plant, idle, then follow a waypoint path: the position changes
  over ticks and no stop happens in D's files.
* E: run: infantry bots fire at each other (projectile/shot events in the server log, no stop).
* F: F.1: bots tick for a full round on foot without stops in urgency code; F.2: medics, engineers
  and support behave (revive, repair, C4, change into a vehicle).
* All: `bf2_aidll` and `bf2` build and link (strict); vtable check green; ELF address on every
  body; new stops only as named frontiers.

### Wave 2

| Pkg | Scope | Fns | Insns |
| --- | --- | ---: | ---: |
| **G vehicles** | all vehicle BB/BBP (Fire3d, Fire2d, HeliFire3d, FireLargeBore, FireUnarmed, GotoWaypoint2d/3d/Boat, AvoidCollision2d, ChangeLandingCraft/Teleport, RunOverSoldier, MoveToFixed), 28 vehicle entries, Boat/Car/Heli/Plane/Tank/CommonControls, 13 vehicle actions/conditions/wrappers | 534 | 87.5k |
| **H deferred** | not reached from the shipped content: TakeCover family (no `setVehicleBehaviour` uses it), BBPFire2dDriver and BBFireArtilleryDriver, Roam, Suicide, CallForMedic, BAPDebugger/PrettyPrinter, unused statements and conditions (Turn, Roll3d, Sense, Evaluate, Debug, FinishedWrapper, ...) | 481 | 42.8k |

G should split in two once it starts: ground and boat (Tank, Car, Boat, LandingCraft and the
2d patterns, about 40k) and air (Plane, Heli and the 3d patterns, about 47k). G is needed for a
real dalian_plant coop round as soon as F.2 lets bots enter vehicles. Until then the vehicle
urgency paths are a known stop.

### M (ai-main, Main/; not a new agent)

`BBCreateFunction*::operator()` (26 functions, 5.3k insns; the string-to-class map is in the
inventory output, e.g. `BBPGotoWaypointInfantery` builds `BBPGotoWaypointSoldier`),
`AIConsole::setVehicleBehaviour` (761 bytes), `getBAPEntryFromName`, `addInterpreterEntry` x2,
`activateDefaultInterpreter`, `createBehaviourModifiers`/`setBehaviourModifier`,
`setVehicleDefaultBehaviour`; and `AISettings::addBehaviour`, `setDefaultBehaviour`,
`addInterpreterEntry` x2, `activateDefaultInterpreter`, `setVehicleMods`,
`prepareBehaviourPool`, `preparePlanInterpreterPool`. These need only S0's headers, so ai-main
can start right after S0.

### Ownership changes to agree before S0

* `AIPlanInterpreter/` (ai-nav): BAPInterpreter, BAPContext and PlanInterpreter go to A; Entry*
  and the infantry controls go to B; the vehicle entries and controls go to G. ai-nav keeps
  pathfinding, knowledge, AIUtils and `AIBotBehaviour/{UrgencyMerger,UrgeCurve,CustomCurve}`.
  Note: the image has UrgencyMerger in the AIBot block and CustomCurve under AIUtils; no move is
  proposed now.
* `AIBotBehaviour/`: the new BB*, UC* and BotBehaviour files belong to A, D, F and G as listed.
* New `AIBotActionPlan/` and `AIBotBehaviourPattern/`: owned file by file per the CSV `package`.
* The skeleton agent is the only one that edits the CMake lists and `aidll.cmd`.

### Order and critical path

S0 comes first. After it, A, B, C, D, E, F and M run in parallel. The first runtime check
(stand and move) needs A, B, C's stand-move half and D, plus M and ai-bot's BotNavigation and
Aimer work. Shooting adds E (the largest single item, about 31k instructions in one file) and
C's shoot half. E is on the critical path. Start it as early as possible, possibly at higher
effort. A full coop round also needs F.1 (urgency of all infantry behaviours), then F.2 and G
once bots take vehicles.

## 5. Dependency graph (packages)

Edges are "uses a class of", counted from direct calls, constructor calls and base classes
(classes CSV `uses_behaviour_classes`, `base`):

```
S0 <- everything (bases, inline classes)
A  <- C (12 base uses), D (25), E (11), F (86), G (69), H (60), M (3)
C  <- B (4), D (16), E (42), F (67), G (67), H (41)
D  <- E (3: BBPFire base, BBFire), G (8), H (3), M (8)
F  <- G (24), H (7), M (19)
E, G, H <- M (create functions)
```

All of these edges are header-level, so with S0 in place they do not order the work. They do
order run validation: A and C must be done before D or E can be run-tested.

## 6. Content (read-only, `$BF2_LINUX_SERVER_DIR/mods/bf2/`)

* `ai/aibehaviours.ai` (802 lines) is the main input:
  * `aiSettings.setMaxNumberOfBehaviours 13` plus `setBehaviour id name` (Avoid, MoveTo, Idle,
    Fire, TakeCover, Change, Special, Revive, c4, Special2, Special3, Random, Triggerable).
  * `setVehicle` (15 types: Tank, Plane, Boat, Infantery, Fixed, Car, LandingCraft, Passenger,
    LandingCraftPassenger, LandingCraftFixed, Helicopter, TransportHelicopter, ArmedCar,
    ManDown, ParatrooperFixed) and `setVehicleClusterMapping`.
  * `createUCConstant UCUnion 1.0`, `createUCLinear UCFire -0.22 1.3`, `UCCallForMedic`,
    `createUCXInverse UCScout 2.5 0.9 1.0 0.5`.
  * 6 `createBehaviourModifiers` sets with `setBehaviourModifier` (77 lines; UnitWeights,
    HeliMoveWeights, StandardWeights, PlaneWeights, ...).
  * 80 `setVehicleBehaviour <vehicle> <behaviour> <createFn BBx> <pattern BBPy> <queue> <curve> <modifiers>`.
    For Infantery: Avoid/BBAvoid/BBPAvoidCollisionInfantery, MoveTo/BBMoveTo/BBPGotoWaypointInfantery,
    Idle/BBIdle/BBPIdleInfantery, Fire/BBFireInfantery/BBPFireInfantery, Special/BBMedicAssist,
    Special2/BBBridgeRepair, c4/BBC4, Special3/BBDefuseMines, Change/BBChange, Revive/BBRevive,
    Random/BBRandom, Triggerable/BBTriggerable. Pattern names that are no class (`UnRelated`,
    `BBPFireDriveAttack`) go with BBFireUnarmed and BBMoveToFixed, which take no pattern.
  * 15 `setVehicleDefaultBehaviour` (all Idle).
  * 132 `addInterpreterEntry <vehicle> <entry> [Medium]` (entry = class name without `Entry`).
  * 320 `activateDefaultInterpreter <vehicle> BAPST<type> Full`.
  * 9 `addSensingAgentPattern`, `setSensingAgentPatternName`/`NullPattern`, `setVehicleMods`,
    `setVehicleMaterialCost`, `setStandardPersonality`, `setManDownEquipment`.
  * A comment in the file warns: "NEVER ALLOW IDLE's urgency to become 0. The AI will CRASH in
    that case."
* `ai/aidefault.ai` (`setMaxNBots 64`, `setBotSkill 0.4`, runs `BotNames.ai`, `AIPathFinding.ai`)
  and `ai/soldier.con`/`vehicle.con` (`game.createAndSpawnAIPlayer`, `ai.addBot ... StandardWeights`).
* `objects_server.zip`: 133 `*/ai/Objects.ai` (`aiTemplate*`, `aiTemplatePlugIn.create
  Unit/Physical/Mobile/ControlInfo/Armament/Cover`). The 99 `ControlInfo` plug-ins
  (drive/aim/look controls, sensitivities, PID constants, look-ahead, scales) feed
  `IPIControlInfo`, which the Entry/Control code reads. Example: `soldiers/Common/ai/Objects.ai`
  (`SoldierCtrl`: PIMouseLookX/Y, PIThrottle, proportional -7, angleSpeed 0.4, derivative -0.01).
* `ai/aidefaultstrategies*.ai` belong to the strategic side (ai-strategic/ai-main), not to this plan.

Taken together, the packages must accept every create-function name and pattern name above
(`pattern_map`/`create_bb` printed by the tool), the 41 entry names, the 44 `BAPST*` names and
the `Full`/`Medium` LOD words.

## 7. Open points and risks

* Rapid type analysis misses targets that are reached only through function pointers. The
  `reach` column is a lower bound for that path and an upper bound for vtables (all slots of an
  instantiated class count as reached).
* Behaviour objects are shared per vehicle type, and per-bot state sits in the Bot (BotMain
  `m_behaviourUrgencies`, the BAPContext variables). Package owners must check which object
  holds each piece of state before naming members.
* The minimum set calls BotMain/Bot through the vtable often. Those slots are not listed here;
  ai-bot's BotMain.h is the reference, and stops there are requests to ai-bot.
* `BBPFireInfantery::createPlanInternal` is one 76 KB function. Plan E with a running ELF
  annotator (`out/re/fn.py` from ai-strategic) and in pieces by branch.

## 8. S0 result and Wave 1 package briefs

### What S0 put in the tree (branch `mac/ai-s0-skeleton`)

* `tools/macos/ai_behaviour_skeleton.py` (one-shot generator, already run; do not re-run over
  owned files) wrote one header and one `.cpp` per attested TU under `AIBotActionPlan/`,
  `AIBotBehaviourPattern/`, `AIBotBehaviour/` and `AIPlanInterpreter/`. Each class header says
  `// ELF: vtable ..., operator new ..., package X.`; that line is the ownership record.
* Virtual functions are declared in ELF slot order. Destructors of root classes sit at their
  slot: `BotScript` execute then D1/D0, `BAPCondition` evaluate, getConditionType, streamSelf
  then D1/D0. Every method has its ELF address in a comment.
* Return types: overrides take the base declaration; the base signatures are hand-set (table
  `HAND_RET` in the generator); everything else is marked `[INFERRED]` with the evidence
  (callee epilogue: x87 = float/double, `al` = bool, `eax` = int; call sites: `testb %al`,
  x87 consumers, dereference = pointer, emitted as `void*` with `[UNRESOLVED] pointee`).
  **Owners verify every `[INFERRED]` return type of their classes** (a pointer that came out as
  `int` is the common error).
* Data members: none, except the frozen base layouts (BAPStatement, BAPCondition, BotBehaviour,
  BBPPattern) and `BAPVariable<T>`. Each class says `[UNRESOLVED] data members`; the owner names
  them from its constructors.
* Bases that a generated constructor stub must construct but that have no default constructor
  got a `[TEMP]` protected default constructor that stops by name
  (`[RETIRE-WHEN: ai-behaviour-skeleton-ctors]`). When you recover a derived constructor, call
  the real base constructor; the last package to do so removes the `[TEMP]` one.
* Hand-written: `AIBotActionPlan/BAPTypes.h` (BAPStatementType 0..44 named after the `BAPST*`
  strings, BAPConditionType 0..58, forward enums), `BAPVariable.h`, `BAPConOpLessThan.h`.
* `AIPlanInterpreter/BAPEntries.{h,cpp}` was split into the 42 attested `Entry*.{h,cpp}`
  (bodies unchanged); `BAPEntries.h` now only includes them. EntryPlaneRoll and
  EntryTurnToMedium were added (H). `BAPEntry::~BAPEntry` is inline (weak in the image).
* Build lists: all new `.cpp` files are in `CMakeLists.txt` (`bf2_aidll_only_sources` and
  `bf2_aidll_sources`) and `Code/Build/aidll.cmd`. **No package edits these lists.**
* Vtable check: `tools/macos/ai_behaviour_vtables.py --build-dir <dir>` compiles a probe with
  the bf2_aidll flags and `-fdump-vtable-layouts` and compares every class with
  `tools/macos/ai_behaviour_vtables.json` (ELF slots). It runs inside the `bf2_vtable_audit`
  gate. Two pure slots of BAPAMoveToDirection (+0x28/+0x2C) are not attested by any override
  and are left out (`unresolved_pure_slots`).
* Not generated: `VehicleUrgencyCalculator.cpp` (anonymous-namespace helpers only; A adds them
  where its callers need them) and the per-class `operator<<` free functions (each owner adds
  its own in its TU).

### S0 validation (2026-11-01)

* `bf2_aidll` and `bf2` build and link strictly (x86_64, `-j2`, fresh tree from cdcb469; 253 new
  `.cpp` files, 0 errors). `bf2_vtable_audit` passes, including the new AI check:
  250 classes (the 222 generated ones, the 42 recovered entries, minus header-only templates)
  with 0 mismatches.
* Server run: `loopback.py --build ai-s0 --ai --bots 16 --no-client --client-seconds 120`:
  map load, pregame, status `playing` after 8.6 s, then 120 s of play with no `[FRONTIER]`
  stop, and the harness quits cleanly. No existing code path calls into the new classes yet
  (the console commands and BotMain's tick still stop or are unregistered on their side), and
  the split entry bodies are byte-identical, so there is no behaviour change by construction.
* `git merge-tree` against mac-metal ea60c5e: clean.

### Common rules for every Wave 1 package

* Own files: the headers and `.cpp` files whose classes carry your package letter (list below
  and the CSV `package` column). Members, private helpers, statics, `#include`s and return-type
  corrections inside them are yours.
* Shared, frozen after S0: the base layouts named above and `BAPTypes.h`. A change goes to
  `coord/ai-requests.md` addressed to `ai-A` (core) and the coordinator.
* Calls into another package's class: use the declared signature. If it is wrong (return type,
  constness), file a request to the owner; do not edit their header. If you are blocked, keep
  your caller and stop by name at the call.
* Evidence: ELF address on every body, AT&T x87 operand caveat (fsub(r)p/fdiv(r)p operands are swapped in AT&T syntax),
  `out/re/fn.py` from ai-strategic for annotated listings, PE `AIDLL.dll` for ABI questions.
* Build: `bf2_aidll` and `bf2` with `-j2`; the `bf2_vtable_audit` gate must stay green (it
  includes the AI vtable check).

### A core (12.2k insns; 18 classes)

* Files: `AIBotActionPlan/{BAPStatement, BAPCondition, BAPCombinerSerial, BAPCombinerParallel,
  BAPFlowControl, BAPFlowCIf, BAPFlowCWhile, BAPFlowCWait, BAPFlowCEnd, BAPConAnd, BAPConOr,
  BAPConNot, BAPConSharedCondition, BAPConTime, BAPConIteration}`, `AIBotBehaviour/BotBehaviour`,
  `AIBotBehaviourPattern/BBPPattern`, `AIPlanInterpreter/BAPInterpreter`, plus the inline
  classes of S0 that still have stops (BAPCombiner, BAPBoleanOp, BotScriptDebugger) and
  `BAPVariable.h`/`BAPTypes.h` upkeep. The base layouts are yours to complete (with a request
  note when a field changes).
* Entry points: `BAPInterpreter::BAPInterpreter(int)` 0x2A95E0, `addEntry` x2 0x2A9D10/0x2A9D50,
  `removeEntry` 0x2A9E10, `activateEntry` 0x2A9ED0 (load, called by ai-main's AISettings);
  `BAPInterpreter::execute` 0x2A99D0 (tick, from `BotMain::planExecution`); the combiners' and
  flow nodes' `execute`; `BotBehaviour` ctor 0x225250, getUrgency/updateActiveUrgency/
  generatePlan 0x225C30 (tick, from `BotMain::decisionMaking` through UrgencyMerger);
  `BBPPattern` ctor 0x296FD0, isInitiated/unInit/forceInitiated.
* Depends on: S0 headers; ai-main (M) for the console registration; ai-bot for the Bot slots
  `+0x118` (dead/inactive test) and `+0xC` (LOD) used by `execute`, and `+0xE8` (bot index).
* Acceptance: (1) `aibehaviours.ai` interpreter lines load (with M) without a stop; (2) a bot
  tick passes `planExecution` with an empty or Idle plan; (3) combiner/flow semantics follow the
  ELF (document the completion rules in the doc); (4) gate green.
* State (2026-11-01, mac/ai-w1-a-core): all A classes except VehicleUrgencyCalculator are recovered;
  base layouts, completion rules and open items are recorded beside the code.

### B entries (10.3k insns; 13 infantry entries + controls)

* Files: `AIPlanInterpreter/{EntryInfanteryMoveTo, EntryInfanteryMoveToObject,
  EntryInfanteryMoveToDirection, EntryInfanteryResetControls, EntryMoveToMediumSoldier,
  EntryMoveToObjectMediumSoldier, EntryInfoWrapper, EntryTrigger, EntryTriggerContinously,
  EntryMouseTurretAimAt, EntryMouseTurretLookAt, EntrySense, EntrySoldierPose}` and the free
  functions in `InfanteryControl`, `MouseControl`, `ControlFunctions`.
* Entry points: each `Entry*::execute(BotScript*, Bot*, bool&, bool&)` (dispatched by
  `BAPInterpreter::execute` when the `.ai` registered the entry for Infantery); helpers such as
  `updateInfantryMoveToPosWithObstacles` 0x2CBD70, `infanteryResetControls` 0x2CB660,
  `mouseControlLookAtDirection` 0x2CCEE0, `regulator` 0x2B1CE0.
* Depends on: C (the statements the entries cast to: BAPAMoveTo*, BAPATrigger*, BAPALook*,
  BAPASoldierPose, BAPInfoWrapper), ai-nav's `IPIControlInfo` accessors (12, request filed by the
  plan), ai-bot's `io::PlayerInput` access on Bot.
* Acceptance: the 12 infantry `execute` stops gone; no stop in the control helpers on an
  infantry run; gate green.

#### B status and continuation notes (ai-w1-b-entries, 2026-11-01)

Branch `mac/ai-w1-b-entries`. Every function of the brief has an ELF-derived body (no
`BF2_AI_FRONTIER` left as a whole body): ControlFunctions (16), InfanteryControl (6),
mouseControlLookAtDirection, and `execute` of all 13 entries (EntryInfanteryMoveToDirection was
already the retail `return true`). Method: llvm-objdump in Intel syntax (no x87 operand swap) plus
the Ghidra decompilation through rea (Ghidra addresses = ELF + 0x10000), checked against each
other at the branches; constants from rodata with 9 digits. Also recovered (shared, engine Math):
`getSignedDiffAngleToDir`, `getSignedDiffAngleToDirXZ/YZ` (Math/Vec3, 0x36C470/0x36C3B0/0x36C410).
Shared-header lines added: C's identical `Bot::getInput()` and `Information::getControlInfo()`.

Open: the reads/writes of fields other packages own stop by name in small file-local bridges
(`*Fields(...)`, `controlXxx(...)` etc.). When the owner's names reach mac-metal, replace the
bridge body (or the call) and nothing else:
* C: BAPAResetControls m_reset* (EntryInfanteryResetControls), BAPATrigger / BAPATriggerContinously
  fields (EntryTrigger*), BAPALook fields (EntryMouseTurretLookAt; +0x18 = allowed deviation,
  +0x1C last yaw error, +0x20 last pitch error), BAPAAimAt +0xA0/+0x110/+0x114[10]/+0x13C and
  getVectorToTarget returning bool, BAPASoldierPose +0x48, BAPASense +0x14/+0x18, BAPInfoWrapper +0x14.
* ai-nav: the five IPIControlInfo forwarders in MouseControl.cpp.
* ai-bot: SquadMember::getMaxSpeed (0x1B5620), BotNavigation obstacles (+0x58),
  SensingAgent move direction (+0x50 Vec2, +0x7C double = 0), BotMain::m_fireCorrection
  (+0xB4; FireCorrectionData +6/+0x20/+0x38).
Not run-validated: bots do not spawn without a human client (coordinator note); acceptance so far
is the strict bf2_aidll/bf2 link and the vtable gate.

### C actions (26.3k insns; 53 classes)

* Files: the `AIBotActionPlan/` headers marked `package C-actions` (BAPA* moves, aims, looks,
  triggers, soldier pose, reset controls, the minimum-set conditions, BAPInfoWrapper and the
  IW*/IC* wrappers, BAPWrapperAiming/LookAt, and the intermediate bases BAPConObject,
  BAPConWeapon, BAPConnectedCondition, BAPInterruptControl, BAPAComponentSoldierPose).
* Order: first the 7 SensingAgent classes (BAPALook, BAPALookAtObject, BAPALookInDir,
  BAPAMergeSoldierInputs, BAPAResetControls, BAPATriggerContinously and the combiner from A),
  because ai-bot's SensingAgent patterns build them on every bot; then stand-move
  (BAPAMoveTo/MoveToFinding/MoveToObject/MoveToObjectFinding, ComponentSoldierPose,
  SoldierPose, Trigger, SetSensingAgentFocus, InfoWrapper, IWIdle, IWMoveTo, ConObjectMoved,
  ConPosition, ConWeaponIsZoomed); then shoot (CSV `min_tier` = shoot).
* Entry points: constructors (called by D/E patterns and SensingAgent), `execute`,
  `evaluate`, `getControls`; the BAPAMoveTo virtual block +0x20..+0x54 used by the entries.
* Depends on: A's bases; ai-bot (BotNavigation `initPathfinding` x2, `updatePath`,
  `getSecondPoint`..., Aimer), ai-nav (TraceUtils, IgnoreListPredicate).
* Acceptance: no stop in C files on a stand-move run, then on a shoot run; gate green.

* **Status 2026-11-01 (ai-w1-c-actions, second session):** all 53 classes recovered (shoot tier
  with ai-bot's Aimer); open: BAPAMoveToObjectFinding::initObjectFinding and four one-call bridges
  to ai-nav/ai-main work not yet in mac-metal (IgnoreListPredicate, IPIControlInfo camera
  accessors, AIStreamOperators). Audit of the return-dropping decompile filter: one more fix
  (BAPAMoveTo::getTargetDir).

### D behaviours (15.1k insns; 10 classes)

* Files: `AIBotBehaviour/{BBIdle, BBMoveTo, BBFire, BBFireInfantery}`,
  `AIBotBehaviourPattern/{BBPIdle, BBPIdleInfantery, BBPIdle2d, BBPIdle3d,
  BBPGotoWaypointsInfantery (BBPGotoWaypointSoldier), BBPFire}`.
* Entry points: constructors (load, from ai-main's `BBCreateFunctionIdle/MoveTo/FireInfantery`),
  `calculateUrgency` (BBFire 0x208040 is 2,089 insns), `chooseWeapon` 0x20A350, `reset`,
  `isAplicable`; `createPlan` of BBPIdleInfantery 0x28F650 and BBPGotoWaypointSoldier 0x284730;
  BBPIdle2d/3d `init` (called by BBPIdleInfantery).
* Depends on: A (BotBehaviour, BBPPattern, combiners), C (the actions the plans build), ai-bot
  (waypoints, squad member state).
* Acceptance: coop bots spawn on dalian_plant, idle, then walk a waypoint path (positions change
  over ticks); no stop in D files.

### E fire pattern (31.6k insns; BBPFireInfantery + Data, FiringState)

* Files: `AIBotBehaviourPattern/BBPFireInfantery`.
* Entry points: constructor 0x25BD00 (load), `createPlan` 0x25C8F0, `createPlanInternal`
  0x2650F0 (18,046 insns), `createGuidedPlanInternalInfantry` 0x25F740, `createFirePlan`
  0x277CF0, `createFirePlanGuided` 0x279B40, `calculateAwayPosition` 0x27B6F0, the `init`
  overloads.
* Depends on: D (BBPFire base), A, C (42 C classes), `BAPVariable<T>` (layout in S0).
* Acceptance: infantry bots fire at enemies (shot/projectile events in the server log); no stop
  in E's file. Work branch by branch; commit per completed helper.

### F-urgency (Wave 1 part of F: about 10.9k insns of urgency + the constructors)

* Files (this part only): `AIBotBehaviour/{BBAvoid, BBMedicAssist, BBBridgeRepair, BBC4,
  BBDefuseMines, BBChange, BBRevive, BBRandom, BBTriggerable}` (all methods except what only
  `generatePlan` reaches), plus the constructors and destructors of their patterns in
  `AIBotBehaviourPattern/BBP{AvoidCollision, AvoidCollisionInfantery, MedicAssist, BridgeRepair, C4,
  DefuseMines, Change, Revive, Random, Triggerable}` (those ctors run at map load).
  The `createPlan` bodies of these patterns stay frontiers until the rest of F.
* Entry points: `calculateUrgency` of each class (BBChange, BBMedicAssist, BBRevive the
  largest), `calculateGiveAmmoUrgency`, `isApplicable`, helpers (`getBridgeCoordinate`,
  `collisionPredicted`, `runwayClear`, `isCallingVehicle`); every bot evaluates them on every
  decision tick through UrgencyMerger.
* Depends on: A (BotBehaviour), ai-bot/ai-strategic (squad, strategic objects, bridges, mines).
* Acceptance: a full coop round on foot runs without a stop in these urgency functions; a
  behaviour other than Idle/MoveTo/Fire is never chosen while its `createPlan` is a frontier
  (if it is, the stop names it, and that is the next work item).

### D behaviours: status (ai-w1-d-behaviours, branch `mac/ai-w1-d-behaviours`)

Recovered (90a3cc6): constructors/destructors of all ten D classes, BBIdle (calculateUrgency,
waitForPickup, reset), BBMoveTo::calculateUrgency, BBFire (initializeGenerator x2,
getBBPFeedback), BBFireInfantery, BBPIdle (freePosition, anyBookedPositions, getType), the
idle/waypoint `init`s and `getType`s, BBPIdleInfantery::createPlan, BBPGotoWaypointSoldier::createPlan,
BBPFire (names agreed with E), and `CustomCurve` (ctor/dtor/init/calculate, in ai-nav's
AIBotBehaviour/CustomCurve.*, because BBFire's ctor builds one at map load).

BBFire::calculateUrgency (0x208040) is recovered around its core: the `!recalculate` arm, the
minute-wise feedback cleanup, the equipment checks, the strategic-area factor, the candidate filter
over `Bot::getSensedObjects()`, and both result arms (best target: chooseWeapon, initializeGenerator,
DecleiningSlopeCurve; none: unInit, urgency 0). The per-candidate scoring (0x208B63..0x209850,
about 1,100 instructions; locals: bestScore -0x198, indirectScore -0x19C, currentTargetScore
-0x1B0, keepCurrentTarget -0x1A9, bestDistance -0x208, best WeaponInfo -0x204, per-armour-class
strengths -0x138[8]) stops by name "(target candidate scoring 0x208B63)" when a sensed hostile,
living object passes the filter. A bot that senses no enemy gets the image's result.

Open: that scoring block, BBFire::chooseWeapon (0x20A350, 1,258 insns; only reached from the
scoring), BBPIdle2d/3d::createPlan (vehicle tier).

Continuation notes:
* Listings: `out/re/d/` (fn.py output per function, `nodbg.py <file>` strips the checkPointer
  Debug blocks; `cc.py <file.cpp>` syntax-checks with the bf2_aidll flags of `out/build/ai-w1-d`).
* Callees that are not declared yet go through `AIBotBehaviour/BBPendingCalls.h` ([TEMP] stop
  wrappers, one per callee, with the requests in coord/ai-requests.md). When an owner lands its
  declaration, replace the wrapper's use by the real call and delete the wrapper.
* Base fields: after merging mac-metal a8acc41, D uses A's names (`m_targets`, `m_pattern`,
  `m_targetChanged`, `BotScript::m_owner`); `bbpending::patternIdOf` reads the protected
  `BAPStatement::m_patternId` through a pointer-to-member (no stop any more).
* Slot maps: IAISettings has its destructor pair at the END (slot 0 = getMaxNBots); ISAI counts
  the destructor pair first (getBotOrderedPos is +0xB8). Bot/BotMain slots are commented in
  AIBot/BotMain.h (`out/re/d/bot_slots.txt`).
* Radio message/channel numbers stay integers (enumerators unresolved; the airadiomessages.ai
  order does not match the image's numbers).

Validation (1c9d716, after merging mac-metal a8acc41): `bf2_aidll` and `bf2` build and link
(x86_64, -j2, fresh `out/build/ai-w1-d`), AI vtable check 250 classes / 0 mismatches. No run:
bots do not spawn without a human client (coordinator note), and the loopback ports were held by
another agent's run at the time.
### F-urgency result (branch `mac/ai-w1-f-urgency`, 2026-11-01)

Recovered from libaidll.so, every body with its ELF address (commits 9714a42..83ead27):

* All nine BB classes: ctors (C1/C2), dtors (D0/D1/D2), `calculateUrgency`, `isApplicable`/
  `isAplicable` and every helper on the urgency path (BBChange: checkBookings, modifyForDriver,
  runwayClear + its anonymous `RunwayObstructedPredicate`, getRadioStrength, isMannedByEnemy,
  isUpsideDown, isBailAllowed, isCallingVehicle, denyGetIn, botChoiceInVehicle; BBMedicAssist:
  isHumanInTarget, vehicleRequestsEngineer, calculateGiveAmmoUrgency; BBBridgeRepair:
  getBridgeCoordinate; BBAvoid: collisionPredicted; BBTriggerable: unInitialise + static
  `m_triggerables`).
* All ten patterns: ctors, dtors, and the methods the urgency code calls directly (`init`
  overloads, BBPChange::setGetInConfirm, BBPMedicAssist::humanPlayerInTarget,
  BBPAvoidCollisionInfantery::init 0x22C870). Data members named from the ctors; sizes match
  `operator new`.
* Remaining frontiers in F files: the ten `createPlan` bodies plus BBPChange
  findValidNavPosToAir/getPosAroundAircraft (F.2), and 25 narrow named stops at calls into
  other packages, all filed in `coord/ai-requests.md` (2026-11-01 ai-w1-f-urgency lines):
  ai-bot (requestResponseModifier, SquadCoordinator isValidPosition/isRepairRequested/
  getRepairRequestPos, SquadMember isTakingFlag/getIsIdle, CommunicationAgent getReceivedMessage/
  receivedMessageExists/sendImmediately/generalConfirmReceivedMessage + a +0x60 accessor,
  BotNavigation getGrenadeWarning (bool) + +0x58/+0x114 accessors, Bot::getC4 by value,
  Bot slots +0x1F8/+0x1FC), A core (calculateVehicleUrgency, closeAndShouldAvoidVehicles,
  BotBehaviour +0x08/+0x0C types), ai-strategic (getClosestDestroyedBridgeSegment returns a
  pointer), ai-main (IAIMineManager::getPathfindingPositionOfMine returns const Vec3&),
  ai-nav (IAIInformationEnvironment::getHardware returns float).
* Every stop has the swap line in a comment (file-local `frontier_*` wrappers or a stand-in
  value after the stop). Swapping is the next F.1 step once the owners land their parts.
* Which stops a tick reaches first: BBAvoid (every soldier, BotNavigation +0x58 reader), so
  until ai-bot adds that accessor no bot passes decision making. Next come BBChange
  (calculateVehicleUrgency) and BBMedicAssist/BBRevive (requestResponseModifier).
* Not in this part (plan F row): the ctors/dtors of G's vehicle BB/BBP classes, which also run
  at map load. They are still stubs and belong to G or a follow-up.
* `BBC4.cpp` has a `[TEMP]` local `C4State` mirror of the getC4 result
  (`[RETIRE-WHEN: ai-bot declares it]`).
