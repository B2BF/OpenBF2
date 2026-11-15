// [CHOSEN, not recovered] Archived with the Novint Falcon (see README.md): the force inputs that were
// declared in Game/Objects/Game/SoldierHapticProfile.h (ledger N2 17b). GenericFireArm declared
// `friend struct HapticWeaponAccess;` for weaponType.
#ifndef GAME_OBJECTS_GAME_SOLDIERHAPTICFORCES_H
#define GAME_OBJECTS_GAME_SOLDIERHAPTICFORCES_H
#include "../../../Core/BuildConfig.h"
#if !DEDICATED
#include "../../../Math/Vec3.h"
namespace dice { namespace hfe { namespace world {
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
