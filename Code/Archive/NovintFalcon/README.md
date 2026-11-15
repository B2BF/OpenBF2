# Novint Falcon (archived)

`[CHOSEN, not recovered]` This directory keeps the reconstructed Novint Falcon support. No
image builds it. The owner decided to drop Falcon support
([issue #17](https://github.com/kr4uzi/OpenBF2/issues/17)). The code is kept as a reference
here, not deleted. Windows later shipped the x86 client again, for PunkBuster. The Falcon
stays archived there too, and the x86 client could load the 32-bit DLL again if restored (see
"Restoring").

## What it was

Retail `BF2.exe` drives a Novint Falcon, a 3-DOF haptic controller, through the vendor SDK
`NovintHFX.dll`. It imports that DLL statically, so retail needs the DLL even when no Falcon
is connected. The game uses it in two ways:

- **As an input device.** `FalconDevice` is the `IDFFalcon` device of the DirectInput device
  manager (`IAdvancedInputDevice`, 27 slots, no destructor slot). It polls position and
  buttons, and in menus it moves the pointer (`g_falconMouseMode`). In game it holds a
  "control box" spring effect, rebuilt from `HapticSettings` whenever
  `g_falconControlBoxDirty` is set.
- **As force feedback.** About 140 game-side lines create and update HFX effects: the camera
  `ShakeForce`, the soldier and vehicle `CraftPhysics`, the soldier `Recoil`, the weapon,
  soldier and kit `RecoilPunch`, and the `Damage` envelope. They also zero the six persistent
  effects on menu entry, death, the free camera and vehicle enter/exit.

`HapticSettings` (`Haptic.con`, the menu's force-feedback page and the profile files) is
retail profile data and is **not** archived. It still loads, saves and applies as in retail.
The Soldier's control-box profile switch (`applyHapticProfile` and `hapticProfileForEntered`
in `Game/Objects/Game/SoldierHapticProfile.cpp`) also stays, because it writes
`HapticSettings`.

`NovintHFX.dll` exists only as a 32-bit DLL. The native x64 client
([issue #7](https://github.com/B2BF/OpenBF2/issues/7)) could never load it. The macOS and
Linux clients never had a Falcon.

## Retail addresses (BF2.exe 1.50)

| Item | Address |
| --- | --- |
| `FalconDevice::~FalconDevice` | `0x6A50D0` |
| `FalconDevice::updateCurrentState` | `0x6A52A0` (ELF `0x8499C80`, sample loop compiled out) |
| `g_falconDevice` | `ds:0xA03940` |
| `g_falconMouseMode` (initialized 1) | `ds:0x978FC4` |
| `g_falconControlBoxDirty` (initialized true) | `ds:0x978FC8` |
| the six force effects `g_hapticForceTargets` | `ds:0x9EBEC8`, `0x9EBECC`, `0x9EBF00`, `0x9EBF04`, `0x9EBF08`, `0x9EBF0C` |
| control-box effect, sample buffer | `ds:0xA03944`, `ds:0xA03918` |
| `FalconControlBoxParams` vtable store after the SDK constructor | `0x6A58A1` (vtable `0x9116E0`) |
| `MainMenuLogic::activate` force zeroing | `0x4A3490..0x4A3535` |
| `Camera::handleUpdate` ShakeForce | `0x5A3F4F..0x5A41D7` |
| `Soldier` ctor / `onExit` / `onEnter` control-box switch | `0x5AC764..0x5AC90B`, `0x5AB5E6..0x5AB83A`, `0x5ACC99..0x5ADE14` |
| `Soldier::handlePlayerInput` Recoil / CraftPhysics | `0x5AE335..0x5AE72B`, `0x5AECE8..0x5AF3F6` |
| `PlayerControlObject::handleUpdate` CraftPhysics | `0x5FAC8C..0x5FBCDB` |
| `GenericFireArm::postFire` kit RecoilPunch | `0x5E2A98..0x5E2CDA` |
| `SoldierBasedRecoilComp::postFire` RecoilPunch | `0x609318..0x609690` |
| `WeaponBasedRecoilComp::postFire` RecoilPunch | `0x60A077..0x60A4FE` |
| `GameLogicDamage::serverGiveDamage` Damage envelope | `0x5E6566..0x5E66A2` |
| `GameLogic` kill tail force zeroing | `0x4ED60B..0x4ED6C2` |
| `FreeCamera::handlePlayerInput` force zeroing | `0x63EC73..0x63ED26` |
| `BF2Engine::renderMenu` mouse mode | `0x40BD44..0x40BD57` |

More addresses are in the comments of the archived sources.

## Contents

The files keep their paths relative to `Code/BF2` (sources) and `Code/Tests` (checks).

| File | Was |
| --- | --- |
| `IO/Input/FalconDevice.h`, `FalconDevice.cpp` | the device, the effect bodies and the globals |
| `IO/Input/FalconHaptics.h` | the game-facing effect helpers |
| `External/NovintHFX/NovintHFX.h`, `NovintHFX.def` | the SDK ABI header and the x86 import-library definition |
| `Game/Objects/Game/SoldierHapticForces.h`, `.cpp` | the force inputs of `SoldierHapticProfile` (`HapticWeaponAccess`, `updateSoldierRecoilForce`, `updateSoldierCraftPhysics`, `updateVehicleCraftPhysics`) |
| `Tests/falcon_update_runtime.cpp`, `Tests/novint_fake/windows.h` | `bf2_falcon_update_check` (Mac, fake SDK) |
| `Tests/falcon_device_runtime.cpp`, `.cmd` | the MSVC x86 check against the stock DLL |
| `Tests/falcon_vtable_restore.cmd` | the MSVC check of the local vtable stores after the SDK constructors |
| `Tests/msvc_x86_layout/novint_x86_layout.cpp`, `include/windows.h` | `bf2_novint_msvc_x86_layout` (clang, MSVC x86 ABI) |
| `Tests/soldier_haptic_profile_runtime.cpp` | `bf2_soldier_haptic_profile_check` as it was, with the force-input checks (the live check keeps the profile switch) |

## What replaced it

Each call site now does what retail does with `NovintHFX.dll` present and no Falcon
connected: `FalconDevice::init` fails, `g_falconDevice` stays null and every effect helper
returns at its null test. The globals were only read by `FalconDevice`, so their writes are
gone too. Each site has a comment that points here.

| Site | Now |
| --- | --- |
| `IO/Input/DX8InputDeviceManager.cpp` `open` | `IDFFalcon` is never opened; its bit stays clear (as in `MacInputDeviceManager`) |
| `Menu/MainMenuLogic.cpp` `activate` | no mouse-mode write, no force zeroing |
| `Menu/BetweenRoundsLogic.cpp` `activate`, `Menu/BF2HudManagerInit.cpp`, `Menu/BF2HudManagerUpdate.cpp` `update`, `Game/Main/BF2Engine.cpp` `renderMenu` | no mouse-mode write |
| `Game/Objects/Game/Soldier.cpp` ctor, `onExit`, `onEnter` | `applyHapticProfile` only (no dirty flag, no zeroing) |
| `Game/Objects/Game/Soldier.cpp` `handlePlayerInput` (Recoil, CraftPhysics) | removed |
| `Game/Objects/Game/PlayerControlObject.cpp` `handleUpdate` (CraftPhysics) | removed |
| `Game/Objects/Base/Camera.cpp` `handleUpdate` (ShakeForce) | removed, as in the ELF |
| `Game/Objects/Base/FreeCamera.cpp` `handlePlayerInput` | no force zeroing |
| `Game/Objects/Weapons/GenericFireArm.cpp`, `RecoilComponents/SoldierBasedRecoilComp.cpp`, `WeaponBasedRecoilComp.cpp` `postFire` (RecoilPunch) | removed |
| `Game/GameLogic/GameLogicDamage.cpp` (Damage) | the look-at inputs are still computed, as in the ELF; no effect |
| `Game/GameLogic/GameLogic.cpp` kill tail | no force zeroing |
| `Game/Main/BF2ClientSeams.h` | no `g_hapticForceTargets` |
| `Code/Tests/camera_shake_runtime.cpp`, `hud_lifecycle_runtime.cpp`, `client_startup_runtime.cpp`, `input_dx8_module_runtime.cpp` and the DX8 input `.cmd` checks | no Falcon expectations, no `NovintHFX.lib` |
| `Game/Objects/Weapons/GenericFireArm.h` | no `HapticWeaponAccess` friend |

The generic `IDFFalcon` lookups in `Menu/BF2HudManagerInput.cpp` (absolute pointer) and
`Game/Main/BF2FrameInterpolator.cpp` (camera interpolation) remain. They now always find no
device, which is the retail no-Falcon branch. The `IDFFalcon`/`IDTFalcon` enumerators, the
`CONTROLLER_IDFFalcon` control-settings name and the control-map token stay, because retail
control maps and profiles name them.

No image imports `NovintHFX.dll`, and no build stages it (client and editor, x86 and x64).

## Restoring

1. Revert the commit "Archive the Novint Falcon integration" (`git revert`). Alternatively,
   move the files above back to their original paths and add the call sites from the table.
2. On Windows x86, restore the import library and staging in the root `CMakeLists.txt`
   (`bf2_novint_imports`, `lib /def:NovintHFX.def /machine:x86`, the `NovintHFX.dll` copy) and
   in `Code/Tools/BF2Editor/CMakeLists.txt`. Add `FalconDevice.cpp` back to the client source
   list, `Code/Build/build.cmd` and `Code/Tests/syntaxcheck.cmd`.
3. Only an x86 client can load the 32-bit DLL. For x64 or macOS, issue #17 describes the
   way back: a 64-bit `NovintHFX` replacement on top of libnifalcon. It needs a working Falcon
   to tune.
