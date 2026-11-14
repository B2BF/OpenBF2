// The Novint control-box profile switch (ledger N2, 17a). See SoldierHapticProfile.h.
#include "SoldierHapticProfile.h"
#if !DEDICATED
#include "../../HapticSettings.h"
#include "../IPlayerControlObjectTemplate.h"
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

void applyHapticProfile(IHapticSettings* s, HapticProfile profile)
{
    switch (profile) {
    case HapticProfileAir:   // 0x5AD363 / 0x5AD87A: +0x64 +0x6C +0xD4 +0xDC +0xE4
        s->setControlBoxLength(s->getControlBoxLengthAIR().x, s->getControlBoxLengthAIR().y, s->getControlBoxLengthAIR().z);
        s->setControlBoxStiffness(s->getControlBoxStiffnessAIR().x, s->getControlBoxStiffnessAIR().y, s->getControlBoxStiffnessAIR().z);
        s->setAimSensitivity(s->getAimSensitivityAIR().x, s->getAimSensitivityAIR().y, s->getAimSensitivityAIR().z);
        s->setAimSensitivityLength(s->getAimSensitivityLengthAIR().x, s->getAimSensitivityLengthAIR().y, s->getAimSensitivityLengthAIR().z);
        s->setTurnSensitivity(s->getTurnSensitivityAIR().x, s->getTurnSensitivityAIR().y, s->getTurnSensitivityAIR().z);
        break;
    case HapticProfileHeli:  // 0x5AD1C9 / 0x5ADA1A: +0x74 +0x7C +0xEC +0xF4 +0xFC
        s->setControlBoxLength(s->getControlBoxLengthHELI().x, s->getControlBoxLengthHELI().y, s->getControlBoxLengthHELI().z);
        s->setControlBoxStiffness(s->getControlBoxStiffnessHELI().x, s->getControlBoxStiffnessHELI().y, s->getControlBoxStiffnessHELI().z);
        s->setAimSensitivity(s->getAimSensitivityHELI().x, s->getAimSensitivityHELI().y, s->getAimSensitivityHELI().z);
        s->setAimSensitivityLength(s->getAimSensitivityLengthHELI().x, s->getAimSensitivityLengthHELI().y, s->getAimSensitivityLengthHELI().z);
        s->setTurnSensitivity(s->getTurnSensitivityHELI().x, s->getTurnSensitivityHELI().y, s->getTurnSensitivityHELI().z);
        break;
    case HapticProfileLand:  // 0x5AD51F: +0x84 +0x8C +0x104 +0x10C +0x114
        s->setControlBoxLength(s->getControlBoxLengthLAND().x, s->getControlBoxLengthLAND().y, s->getControlBoxLengthLAND().z);
        s->setControlBoxStiffness(s->getControlBoxStiffnessLAND().x, s->getControlBoxStiffnessLAND().y, s->getControlBoxStiffnessLAND().z);
        s->setAimSensitivity(s->getAimSensitivityLAND().x, s->getAimSensitivityLAND().y, s->getAimSensitivityLAND().z);
        s->setAimSensitivityLength(s->getAimSensitivityLengthLAND().x, s->getAimSensitivityLengthLAND().y, s->getAimSensitivityLengthLAND().z);
        s->setTurnSensitivity(s->getTurnSensitivityLAND().x, s->getTurnSensitivityLAND().y, s->getTurnSensitivityLAND().z);
        break;
    case HapticProfileSea:   // 0x5AD6C8: +0x94 +0x9C +0x11C +0x124 +0x12C
        s->setControlBoxLength(s->getControlBoxLengthSEA().x, s->getControlBoxLengthSEA().y, s->getControlBoxLengthSEA().z);
        s->setControlBoxStiffness(s->getControlBoxStiffnessSEA().x, s->getControlBoxStiffnessSEA().y, s->getControlBoxStiffnessSEA().z);
        s->setAimSensitivity(s->getAimSensitivitySEA().x, s->getAimSensitivitySEA().y, s->getAimSensitivitySEA().z);
        s->setAimSensitivityLength(s->getAimSensitivityLengthSEA().x, s->getAimSensitivityLengthSEA().y, s->getAimSensitivityLengthSEA().z);
        s->setTurnSensitivity(s->getTurnSensitivitySEA().x, s->getTurnSensitivitySEA().y, s->getTurnSensitivitySEA().z);
        break;
    case HapticProfileSoldier:  // +0x54 +0x5C +0xBC +0xC4 +0xCC
    default:
        s->setControlBoxLength(s->getControlBoxLengthSOLDIER().x, s->getControlBoxLengthSOLDIER().y, s->getControlBoxLengthSOLDIER().z);
        s->setControlBoxStiffness(s->getControlBoxStiffnessSOLDIER().x, s->getControlBoxStiffnessSOLDIER().y, s->getControlBoxStiffnessSOLDIER().z);
        s->setAimSensitivity(s->getAimSensitivitySOLDIER().x, s->getAimSensitivitySOLDIER().y, s->getAimSensitivitySOLDIER().z);
        s->setAimSensitivityLength(s->getAimSensitivityLengthSOLDIER().x, s->getAimSensitivityLengthSOLDIER().y, s->getAimSensitivityLengthSOLDIER().z);
        s->setTurnSensitivity(s->getTurnSensitivitySOLDIER().x, s->getTurnSensitivitySOLDIER().y, s->getTurnSensitivitySOLDIER().z);
        break;
    }
}

HapticProfile hapticProfileForEntered(IPlayerControlObjectTemplate* entered, IPlayerControlObjectTemplate* root)
{
    switch (entered->getVehicleType()) {          // +0x34; jump tables 0x5ADE64 / 0x5ADE70
    case 8: case 9: case 15: case 19: case 20: case 24: case 25:
        return HapticProfileSoldier;
    case 26:
        switch (entered->getVehicleCategory()) {  // +0x24
        case 2:
            return HapticProfileAir;
        case 3:
            return entered->getControlsCategory() ? HapticProfileHeli : HapticProfileSoldier;  // +0x2C
        default:
            return HapticProfileSoldier;
        }
    default:
        switch (root->getVehicleCategory()) {     // +0x24; jump table 0x5ADE84
        case 0: return HapticProfileLand;
        case 1: return HapticProfileSea;
        case 2: return HapticProfileAir;
        case 3: return HapticProfileHeli;
        default: return HapticProfileSoldier;
        }
    }
}

} } }
#endif
