// The Novint control-box profile switch the Soldier makes for the local player (ledger N2, 17a).
// [CHOSEN, not recovered] file and names: retail inlines all of this in Soldier::Soldier
// (0x5AC784..0x5AC90B), Soldier::onEnter (0x5ACCC6..0x5ADE14) and Soldier::onExit
// (0x5AB611..0x5AB83A). It lives here so the fake-settings check can drive it without a Soldier.
#ifndef GAME_OBJECTS_GAME_SOLDIERHAPTICPROFILE_H
#define GAME_OBJECTS_GAME_SOLDIERHAPTICPROFILE_H
#include "../../../Core/BuildConfig.h"
#if !DEDICATED
#include "../../../Math/Vec3.h"
namespace dice { namespace hfe {
class IHapticSettings;
namespace world {
class IPlayerControlObjectTemplate;
class IPlayer;
class IObject;
class GenericFireArmTemplate;
class ICameraObject;
class IPhysicsNode;

// Friend of GenericFireArm. weaponType is SoldierHapticProfile.cpp's read (0x5E0AC0); setTemplate is
// defined only by the check.
struct HapticWeaponAccess {
    static int weaponType(IObject* weapon);
    static void setTemplate(IObject* weapon, const GenericFireArmTemplate* weaponTemplate);
};

// HapticSettings' per-vehicle profiles, in their interface order.
enum HapticProfile {
    HapticProfileSoldier,
    HapticProfileAir,
    HapticProfileHeli,
    HapticProfileLand,
    HapticProfileSea
};

// The five copies every site makes: the active control-box length and stiffness, aim sensitivity,
// aim sensitivity length and turn sensitivity, each from the profile's own value (HapticSettings +0x48,
// +0x50, +0xA8, +0xB0, +0xB8 set from the profile's getters). Each getter is called once per component,
// as retail does.
void applyHapticProfile(IHapticSettings* settings, HapticProfile profile);

// Soldier::onEnter's choice (0x5ACCC6..0x5AD51F): by the entered object's vehicle type; type 26 by
// its own vehicle category (2 AIR, 3 HELI while its controls category is non-zero, else SOLDIER);
// types 8, 9, 15, 19, 20, 24 and 25 SOLDIER; every other type by the root object's vehicle category
// (0 LAND, 1 SEA, 2 AIR, 3 HELI, other SOLDIER).
HapticProfile hapticProfileForEntered(IPlayerControlObjectTemplate* entered,
                                      IPlayerControlObjectTemplate* root);

// Soldier::handlePlayerInput's "Recoil" decision (0x5AE352..0x5AE463, ledger N2 17b), with a device
// connected: only for the local occupant; the weapon's type (0x5E0AC0: the firearm's template
// weaponType, [INFERRED]) is read only when the yaw delta or the raw pitch recoil is non-zero, and
// the weapon is not checked, as shipped; then io::updateRecoilForce. [CHOSEN, not recovered] The
// caller looks up the weapon and the local player up front (both lookups have no effects).
void updateSoldierRecoilForce(IPlayer* occupant, IPlayer* localPlayer, float yawDelta, float pitchRecoil,
                              IObject* weapon);

// Soldier::handlePlayerInput's "CraftPhysics" input (0x5AECF5..0x5AED72, ledger N2 17b), with a device
// connected: only for the local occupant; the physics node's linear integration velocity rotated into
// the frame of the controlling player's vehicle (v * getInvAbsoluteTransformation(), 0x5A1710), then
// io::updateSoldierCraftPhysicsForce. [CHOSEN, not recovered] The caller reads the velocity up front.
void updateSoldierCraftPhysics(IPlayer* occupant, IPlayer* localPlayer, IPlayer* player, const Vec3& velocity,
                               float dt);

// PlayerControlObject::handleUpdate's "CraftPhysics" input (0x5FAC8C..0x5FADAE, ledger N2 17b), for the
// local occupant with a device and a root physics node: the node's positional acceleration is read and
// dropped, as shipped; the positional speed is rotated (0x5A1710) into the frame chosen by ds:0x974BF8
// (initialized 3 and never written: 0 the vehicle's relative, 1 absolute, 3 inverse absolute,
// 4 visual absolute transformation, 2 the camera's getTransformation(0), 5 its orthonormal inverse);
// then io::updateVehicleCraftPhysicsForce with the vehicle category. [CHOSEN, not recovered] The caller
// reads the vehicle, the camera and the category up front (all without effects).
void updateVehicleCraftPhysics(IPhysicsNode* physics, IObject* vehicle, ICameraObject* camera, float dt,
                               int vehicleCategory);

} } }
#endif
#endif
