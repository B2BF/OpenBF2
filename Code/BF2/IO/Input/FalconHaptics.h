// The game-facing half of the Falcon device (ledger N2): the control-box flag and the haptic-effect
// helpers the game code calls. [CHOSEN, not recovered] Split from FalconDevice.h so that game TUs do not
// see NovintHFX.h, which includes <windows.h> (its min/max macros broke Soldier.cpp's std::min/max on
// MSVC). The bodies are in FalconDevice.cpp; on POSIX clients and without a device they do nothing.
#ifndef IO_INPUT_FALCONHAPTICS_H
#define IO_INPUT_FALCONHAPTICS_H
#include "../../Core/BuildConfig.h"
#if !DEDICATED
#include "../../Math/Vec3.h"
#include "../../Math/Mat4.h"
#include <string>
namespace dice { namespace hfe { namespace world { class IPlayerControlObject; class IPlayer; } } }
namespace dice { namespace hfe { namespace io {
// [INFERRED] name, PE978FC8 initialized true. updateCurrentState rebuilds the control-box effect
// from HapticSettings when it is set and clears it; the haptic-effect sites 0x5AB83A, 0x5AC90B and
// 0x5ADD98 (ledger N2) set it.
extern bool g_falconControlBoxDirty;
// MainMenuLogic::activate 0x4A3490..0x4A3535 (inline in retail): one NovintHFX::Vect3(0, 0, 0), then
// each non-null haptic force effect's +0x4C in retail's order 0x9EBF00, F04, F08, F0C, EC8, ECC.
// [CHOSEN, not recovered] Out of line here because MainMenuLogic builds on every platform and the
// SDK declarations are Windows-only.
void zeroHapticForceTargets();
// Camera::handleUpdate's Novint block (BF2.exe 0x5A3F4F..0x5A41D7, ledger N2 17b), for the local
// occupant's camera. With a device: create the two CONSTANT "ShakeForce" effects (ds:0x9EBEC8/CC) once,
// then each frame set Vect3(x, y, 0) on the soldier's (CC) or the vehicle's (C8) from the camera's
// shake terms scaled by HapticSettings (+0x7DC explosion, +0x7E4 speed, +0x7D4 general on the
// close-vehicle pitch, +0x7C4 / +0x7CC on the recoil), each axis capped by the force cap with its sign.
// The soldier test is the occupied object's template class id (pco -> IID_IObject -> +0x14 -> +0x10).
// [CHOSEN, not recovered] Out of line here for the same reason as zeroHapticForceTargets.
void updateShakeForce(world::IPlayerControlObject* pco, float explosionRoll, float speedRoll,
                      float closeVehiclePitch, float recoilY);
// g_falconDevice != 0 (false on POSIX clients), for callers that build on every platform.
// [CHOSEN, not recovered]
bool hapticDeviceConnected();
// Soldier::handlePlayerInput's "Recoil" block (BF2.exe 0x5AE335..0x5AE72B, ledger N2 17b), for the
// local occupant while a device is connected: create the Constant "Recoil" effect (ds:0x9EBF00) once
// (params +0x18 = 10), then set it every tick. Zero unless the yaw delta (recoil plus look) or the
// raw pitch recoil is non-zero; otherwise Vect3(yawScale * x, -(pitchScale * y), 0), where x and y are
// those two values capped by the force caps with their sign (0x5A6280) and the scales are
// HapticSettings' recoil yaw / pitch scales for the weapon type (0 RIFLE .. 12 DEFIB, else 0).
// [CHOSEN, not recovered] Out of line, as zeroHapticForceTargets.
void updateRecoilForce(float yawDelta, float pitchRecoil, int weaponType);
// Soldier::handlePlayerInput's "CraftPhysics" block (BF2.exe 0x5AECE8..0x5AF3F6, ledger N2 17b), after
// setRotationalSpeed, for the local occupant while a device is connected. The soldier's velocity in
// its vehicle's frame goes into static three-entry histories (velocities ds:0x9EC550, tick times
// ds:0x9EC540); once full, the acceleration is (oldest - newest) * (1 / (the last two tick times)),
// else zero. Its length against PhysicsSOLDIERTransitionValueMed / High picks the Low, Med or High
// scales (each times PhysicsSOLDIERScale's component). The three Constant effects "CraftPhysicsX/Y/Z"
// (ds:0x9EBF04/08/0C) are created once with params +0x18 = PhysicsSOLDIERSmoothTimeX/Y/ZLow (Low in
// every band, as shipped); each tick each gets SetSmoothTime(that value) and a force along its own
// axis: -(sx * ax), -(sy * ay), +(sz * az), each acceleration component first capped by the force cap
// with its sign. [CHOSEN, not recovered] Out of line, as zeroHapticForceTargets.
void updateSoldierCraftPhysicsForce(const Vec3& localVelocity, float dt);
// PlayerControlObject::handleUpdate's "CraftPhysics" block (BF2.exe 0x5FADAE..0x5FBCD8, ledger N2 17b):
// the same core with its own histories (ds:0x9F06BC velocities, ds:0x9F06AC tick times) and the
// HapticSettings block of the vehicle category (jump table 0x5FBE40: 1 SEA, 2 AIR, 3 HELI, LAND for 0
// and every other value); unlike the soldier's, each band takes its own smooth times.
void updateVehicleCraftPhysicsForce(const Vec3& localVelocity, float dt, int vehicleCategory);
// SoldierBasedRecoilComp::postFire's "RecoilPunch" (BF2.exe 0x609353..0x609690, ledger N2 17b), for the
// local occupant while a device is connected: punch time and scale from HapticSettings by the soldier's
// active weapon type (jump table 0x60969C: 0 RIFLE .. 12 DEFIB, but 5 and every type above 12 share the
// default, PISTOL); with a random recoil force, time *= the recoil graph's sample count and
// scale = -(((variation + value) * 0.5) * forceModifier * lastSample) * scale; then fireRecoilPunch.
// The last sample is read unchecked, as shipped.
void soldierRecoilPunch(int weaponType, bool hasRandomRecoilForce, unsigned int recoilSamples, const float* lastSample,
                        float forceUpValue, float forceUpVariation, float forceModifier, float fireStrength);
// The shared tail of the RecoilPunch sites (0x6095EF..0x60968A, 0x60A451..0x60A4F8): an Envelope
// params object (+0x18 = the punch time truncated, m_force[2] = capZ * scale when the fire strength's
// magnitude is above the Z force cap, as shipped without its sign, else scale * fireStrength), the
// effect "RecoilPunch" created without keeping it, the params deleted.
void fireRecoilPunch(float punchTime, float punchScale, float fireStrength);
// WeaponBasedRecoilComp::postFire's "RecoilPunch" (BF2.exe 0x60A0E7..0x60A4F8, ledger N2 17b), for the
// local occupant while a device is connected: by the firing weapon's type, 13..19 (jump table 0x60A5C0:
// V_LMG, V_HMG, V_HELIGUN, V_AIRGUN, V_MISSILE, V_ARMORCANNON, V_COAXIALGUN) take that type's punch time
// and scale and set HapticSettings +0x48C / +0x490 (the recoil scales the ShakeForce block reads) to its
// recoil yaw / pitch scale; any other type takes RIFLE's four values when the lower-cased template name
// contains "firingport", else time 1, scale 0 and both set to 0. Then scale *= cameraRecoilSize,
// time *= cameraRecoilSpeed, and fireRecoilPunch.
void weaponRecoilPunch(int weaponType, const std::string& templateName, float cameraRecoilSpeed,
                       float cameraRecoilSize, float fireStrength);
// GenericFireArm::postFire's "RecoilPunch" for a weapon without a recoil component (BF2.exe
// 0x5E2A98..0x5E2CDA, the else of m_recoilComp->postFire; ledger N2 17b), when the weapon's player is
// the local player and a device is connected: weapon types 9..12 (jump table 0x5E2E90: KNIFE, DROP,
// THROWN, DEFIB) take that type's punch time and scale; any other type takes DROP's when the lower-cased
// template name contains "kit" or else "wrench", otherwise time 1 and scale 0; then fireRecoilPunch.
void kitRecoilPunch(int weaponType, const std::string& templateName, float fireStrength);
// GameLogicDamage::serverGiveDamage's "Damage" envelope (BF2.exe 0x5E6566..0x5E66A2, ledger N2 17b):
// for the local victim with a device, the look-at frame's forward row (lookAt +0x20) in the damaged
// owner's frame (dot products with its rows 0, 1, 2) gives Vect3(-a, b, c), normalized and scaled by
// HapticSettings' damage scale * damage * 0.5, into an Envelope params object (+0x18 = 100,
// m_force); the effect "Damage" created without keeping it. The victim test is pointer equality.
void updateDamageForce(world::IPlayer* victim, world::IPlayer* localPlayer, const Mat4& lookAt,
                       const Mat4& ownerTransform, float damage);
} } }
#endif // !DEDICATED
#endif
