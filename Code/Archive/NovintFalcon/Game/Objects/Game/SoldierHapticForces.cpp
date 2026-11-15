// [CHOSEN, not recovered] Archived with the Novint Falcon (see README.md): the force inputs the Soldier
// and PlayerControlObject fed to the Falcon effects (ledger N2 17b). They were part of
// Game/Objects/Game/SoldierHapticProfile.cpp; the control-box profile switch stays there.
#include "SoldierHapticForces.h"
#if !DEDICATED
#include "../GenericFireArmTemplate.h"
#include "../Weapons/GenericFireArm.h"
#include "../../../IO/Input/FalconHaptics.h"
#include "../IObject.h"
#include "../IPlayer.h"
#include "../../../Math/Mat4.h"
#include "../../../Physics/IPhysicsNode.h"
#include "../ICameraObject.h"

namespace dice { namespace hfe { namespace world {

int HapticWeaponAccess::weaponType(IObject* weapon)
{
    return static_cast<GenericFireArm*>(weapon)->m_template->m_weaponType;
}

void updateSoldierRecoilForce(IPlayer* occupant, IPlayer* localPlayer, float yawDelta, float pitchRecoil,
                              IObject* weapon)
{
    if (!occupant || occupant != localPlayer)
        return;
    int weaponType = -1;
    if (yawDelta != 0.0f || pitchRecoil != 0.0f)
        weaponType = HapticWeaponAccess::weaponType(weapon);
    io::updateRecoilForce(yawDelta, pitchRecoil, weaponType);
}

void updateSoldierCraftPhysics(IPlayer* occupant, IPlayer* localPlayer, IPlayer* player, const Vec3& velocity,
                               float dt)
{
    if (!occupant || occupant != localPlayer)
        return;
    const Mat4 inverse = player->getVehicle()->getInvAbsoluteTransformation();   // copied, as shipped
    // 0x5A1710: v * M, accumulated on the x87 stack in this order.
    const double vx = velocity.x, vy = velocity.y, vz = velocity.z;
    Vec3 local;
    local.x = static_cast<float>(inverse.m[8] * vz + inverse.m[4] * vy + vx * inverse.m[0]);
    local.y = static_cast<float>(inverse.m[1] * vx + inverse.m[9] * vz + inverse.m[5] * vy);
    local.z = static_cast<float>(inverse.m[2] * vx + inverse.m[10] * vz + inverse.m[6] * vy);
    io::updateSoldierCraftPhysicsForce(local, dt);
}

namespace {
int s_hapticFrameMode = 3;   // ds:0x974BF8, initialized 3; nothing writes it
// 0x5A1710: v * M, accumulated on the x87 stack in this order.
Vec3 rotateIntoFrame(const Vec3& v, const Mat4& m)
{
    const double vx = v.x, vy = v.y, vz = v.z;
    Vec3 out;
    out.x = static_cast<float>(m.m[8] * vz + m.m[4] * vy + vx * m.m[0]);
    out.y = static_cast<float>(m.m[1] * vx + m.m[9] * vz + m.m[5] * vy);
    out.z = static_cast<float>(m.m[2] * vx + m.m[10] * vz + m.m[6] * vy);
    return out;
}
} // namespace

void updateVehicleCraftPhysics(IPhysicsNode* physics, IObject* vehicle, ICameraObject* camera, float dt,
                               int vehicleCategory)
{
    physics->getPositionalAcceleration();                                       // +0xA8 PE, dropped
    Mat4 frame = Mat4::getIdentity();
    switch (s_hapticFrameMode) {   // the jump table at 0x5FBE28
    case 0: frame = vehicle->getRelativeTransformation(); break;
    case 1: frame = vehicle->getAbsoluteTransformation(); break;
    case 2: frame = camera->getTransformation(0.0f); break;
    case 3: frame = vehicle->getInvAbsoluteTransformation(); break;
    case 4: frame = vehicle->getVisualAbsoluteTransformation(); break;
    case 5: frame.setInverse(camera->getTransformation(0.0f)); break;
    default: break;
    }
    io::updateVehicleCraftPhysicsForce(rotateIntoFrame(physics->getPositionalSpeed(), frame), dt, vehicleCategory);
}

} } }
#endif
